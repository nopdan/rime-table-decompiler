package main

import (
	"context"
	"encoding/binary"
	"fmt"
	"log"
	"os"

	"github.com/tetratelabs/wazero"
	"github.com/tetratelabs/wazero/api"
	"github.com/tetratelabs/wazero/imports/wasi_snapshot_preview1"
)

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintf(os.Stderr, "Usage: %s <wasm-file> <table.bin> [output.yaml]\n", os.Args[0])
		os.Exit(1)
	}

	wasmPath := os.Args[1]
	inputPath := os.Args[2]
	outputPath := ""
	if len(os.Args) >= 4 {
		outputPath = os.Args[3]
	}

	ctx := context.Background()

	wasmBytes, err := os.ReadFile(wasmPath)
	if err != nil {
		log.Fatalf("failed to read wasm file: %v", err)
	}

	inputData, err := os.ReadFile(inputPath)
	if err != nil {
		log.Fatalf("failed to read input file: %v", err)
	}

	result, err := Decompile(ctx, wasmBytes, inputData)
	if err != nil {
		log.Fatalf("decompile failed: %v", err)
	}

	if outputPath == "" {
		os.Stdout.Write(result)
	} else {
		if err := os.WriteFile(outputPath, result, 0o644); err != nil {
			log.Fatalf("failed to write output: %v", err)
		}
		fmt.Fprintf(os.Stderr, "saved to %s (%d bytes)\n", outputPath, len(result))
	}
}

// Decompile loads the wasm module and calls decompile(input_ptr, input_len, out_size_ptr).
// Returns the decompiled YAML bytes.
func Decompile(ctx context.Context, wasmBytes, inputData []byte) ([]byte, error) {
	rt := wazero.NewRuntime(ctx)
	defer rt.Close(ctx)

	// Instantiate WASI for fd_close, fd_write, fd_read, fd_seek
	wasi_snapshot_preview1.MustInstantiate(ctx, rt)

	// Provide env.emscripten_notify_memory_growth (no-op stub)
	_, err := rt.NewHostModuleBuilder("env").
		NewFunctionBuilder().
		WithFunc(func(ctx context.Context, idx int32) {
			// no-op: called when memory grows
		}).
		Export("emscripten_notify_memory_growth").
		Instantiate(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to instantiate env module: %w", err)
	}

	mod, err := rt.InstantiateWithConfig(ctx, wasmBytes,
		wazero.NewModuleConfig().WithStartFunctions()) // disable auto-calling _start
	if err != nil {
		return nil, fmt.Errorf("failed to instantiate wasm: %w", err)
	}
	defer mod.Close(ctx)

	// Initialize C++ runtime (global constructors)
	if ctors := mod.ExportedFunction("__wasm_call_ctors"); ctors != nil {
		if _, err := ctors.Call(ctx); err != nil {
			return nil, fmt.Errorf("failed to call __wasm_call_ctors: %w", err)
		}
	}

	memory := mod.Memory()

	malloc := mod.ExportedFunction("malloc")
	free := mod.ExportedFunction("free")
	decompile := mod.ExportedFunction("decompile")
	decompileFree := mod.ExportedFunction("decompile_free")

	if malloc == nil || free == nil || decompile == nil || decompileFree == nil {
		return nil, fmt.Errorf("missing required exports (malloc/free/decompile/decompile_free)")
	}

	// Allocate input buffer in wasm memory
	results, err := malloc.Call(ctx, uint64(len(inputData)))
	if err != nil {
		return nil, fmt.Errorf("malloc for input failed: %w", err)
	}
	inPtr := uint32(results[0])
	defer free.Call(ctx, uint64(inPtr))

	if !memory.Write(inPtr, inputData) {
		return nil, fmt.Errorf("failed to write input data to wasm memory")
	}

	// Allocate space for out_size (uint32 = 4 bytes)
	results, err = malloc.Call(ctx, 4)
	if err != nil {
		return nil, fmt.Errorf("malloc for out_size failed: %w", err)
	}
	outSizePtr := uint32(results[0])
	defer free.Call(ctx, uint64(outSizePtr))

	// Zero out_size
	memory.WriteUint32Le(outSizePtr, 0)

	// Call decompile(input_ptr, input_len, out_size_ptr) -> output_ptr
	results, err = decompile.Call(ctx, uint64(inPtr), uint64(len(inputData)), uint64(outSizePtr))
	if err != nil {
		return nil, fmt.Errorf("decompile call failed: %w", err)
	}
	outPtr := uint32(results[0])
	if outPtr == 0 {
		return nil, fmt.Errorf("decompile returned null (invalid table data?)")
	}

	// Read out_size
	outSizeBytes, ok := memory.Read(outSizePtr, 4)
	if !ok {
		return nil, fmt.Errorf("failed to read out_size from wasm memory")
	}
	outSize := binary.LittleEndian.Uint32(outSizeBytes)

	// Read output
	output, ok := memory.Read(outPtr, outSize)
	if !ok {
		return nil, fmt.Errorf("failed to read output from wasm memory")
	}

	// Copy before freeing
	result := make([]byte, len(output))
	copy(result, output)

	// Free output buffer
	decompileFree.Call(ctx, uint64(outPtr))

	return result, nil
}

// Reusable module wrapper for repeated calls (optional optimization).
type WasmDecompiler struct {
	rt  wazero.Runtime
	mod api.Module
}

// NewWasmDecompiler creates a reusable decompiler. Call Close() when done.
func NewWasmDecompiler(ctx context.Context, wasmBytes []byte) (*WasmDecompiler, error) {
	rt := wazero.NewRuntime(ctx)

	wasi_snapshot_preview1.MustInstantiate(ctx, rt)

	_, err := rt.NewHostModuleBuilder("env").
		NewFunctionBuilder().
		WithFunc(func(ctx context.Context, idx int32) {}).
		Export("emscripten_notify_memory_growth").
		Instantiate(ctx)
	if err != nil {
		rt.Close(ctx)
		return nil, err
	}

	mod, err := rt.InstantiateWithConfig(ctx, wasmBytes,
		wazero.NewModuleConfig().WithStartFunctions())
	if err != nil {
		rt.Close(ctx)
		return nil, err
	}

	if ctors := mod.ExportedFunction("__wasm_call_ctors"); ctors != nil {
		if _, err := ctors.Call(ctx); err != nil {
			mod.Close(ctx)
			rt.Close(ctx)
			return nil, err
		}
	}

	return &WasmDecompiler{rt: rt, mod: mod}, nil
}

func (d *WasmDecompiler) Decompile(ctx context.Context, inputData []byte) ([]byte, error) {
	memory := d.mod.Memory()
	malloc := d.mod.ExportedFunction("malloc")
	free := d.mod.ExportedFunction("free")
	decompile := d.mod.ExportedFunction("decompile")
	decompileFree := d.mod.ExportedFunction("decompile_free")

	results, err := malloc.Call(ctx, uint64(len(inputData)))
	if err != nil {
		return nil, err
	}
	inPtr := uint32(results[0])
	defer free.Call(ctx, uint64(inPtr))

	if !memory.Write(inPtr, inputData) {
		return nil, fmt.Errorf("write input failed")
	}

	results, err = malloc.Call(ctx, 4)
	if err != nil {
		return nil, err
	}
	outSizePtr := uint32(results[0])
	defer free.Call(ctx, uint64(outSizePtr))
	memory.WriteUint32Le(outSizePtr, 0)

	results, err = decompile.Call(ctx, uint64(inPtr), uint64(len(inputData)), uint64(outSizePtr))
	if err != nil {
		return nil, err
	}
	outPtr := uint32(results[0])
	if outPtr == 0 {
		return nil, fmt.Errorf("decompile returned null")
	}

	outSizeBytes, _ := memory.Read(outSizePtr, 4)
	outSize := binary.LittleEndian.Uint32(outSizeBytes)
	output, _ := memory.Read(outPtr, outSize)

	result := make([]byte, len(output))
	copy(result, output)

	decompileFree.Call(ctx, uint64(outPtr))
	return result, nil
}

func (d *WasmDecompiler) Close(ctx context.Context) {
	if d.mod != nil {
		d.mod.Close(ctx)
	}
	if d.rt != nil {
		d.rt.Close(ctx)
	}
}

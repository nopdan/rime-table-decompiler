# rime-table-decompiler

> fork from <https://github.com/whjiang/rime_table_bin_decompiler>

这个项目可以简单的用于反编译 Rime 的`xxx.table.bin`以生成`xxx.dict.yaml`文本文件。

这个项目中的大部分代码是从 librime 源代码中 copy 过来的。

## Usage

拖动文件到 `rime-table-decompiler` 或者命令行运行：

```bash
#Usage: rime_table_decompiler <rime-table-file> [save-path]
Example: rime_table_decompiler pinyin.table.bin pinyin.dict.yaml
```

不指定`save-path`则自动输出到文件 `xxx.yaml`。

> 注意：因为`xxx.table.bin`没有元数据信息，所以生成的`xxx.dict.yaml`的文件头中的元数据是根据常见的元数据填进去的，可能是错误的。用户需要自己进行修改。

若输出 **"table format version 3 is no longer supported. please upgrade to version Rime::Table/4.0"**，可尝试使用 `rime_table_v3_decompiler`

## Build

编译方法：(需要安装 xmake, unzip 等工具)

```bash
git clone https://github.com/nopdan/rime-table-decompiler
cd rime-table-decompiler
xmake
```

## WASM

使用 emscripten 编译：

```bash
xmake f -p wasm
xmake build rime_table_decompiler_wasm
```

导出函数：

- `decompile(input_ptr, input_len, out_len_ptr) -> output_ptr`
- `decompile_free(output_ptr)`

输出是 UTF-8 的 `yaml` 字节数组，调用方负责释放返回的内存。

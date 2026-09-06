# 演習2

- kfree()で解放したページを1で満たす処理がフリーズしたかと思うほど非常に遅い
- いずれbuddyアロケータに置き換えるのでここではこの処理を削除した
- muslのアセンブリ版のmemset.Sは動かなかった

## 実行

```bash
Starting kernel ...

free_range: start: 42008000, end: 80000000
Allocator: Init success.
check_free_list ok.
```

# 演習3: 割込みと例外

- 割り込み、タイマー、クロックの初期化
- トラップフレームの策定とトラップコードの作成（未テスト）
- TIMER0を計時とインターバルタイマーに、TIMER1はタイマーに使用することにした
- spinlockもここで追加

## 資料

- [ARM Generic Interrupt Controller](file:///Volumes/wdb/docs/pine64/arm_gic_architecture_specification_v2.pdf)
- [CoreLink GIC-400 Generic InterruptController](file:///Volumes/wdb/docs/pine64/gic400_r0p1_trm-1.pdf)

## 実行

```bash
Starting kernel ...

free_range: start: 42009000, end: 80000000
[0]alloc_init: alloc_init ok
[0]irq_init: GICD_TYPER: [15:11] 1f, [10] 1, [7:5] 3, [4:0] 6
[0]irq_init: GICD_IIDR: Product 2, Variant 0, Rev 1, impl 43b
[0]irq_init: irq_init ok
[0]timer_init: timer_init ok
```

## 割り込みが発火していないことが判明

- trap_init()の`extern void *vectors;`が原因でvectorsのアドレスが
  正しくvbar_el1にセットされなかったため、trap()自体実行されていなかった。
- Timer0/Timer1割り込みがGICv2経由では発火しないことがわかった。
- SPI割り込みであるUART0割り込み(GIC_SPI(0))は発火したので、
  すべてのSPI割り込みが発火しないわけではないことが確認された。
- ARM仮想タイマー（GIC_PPI(11)=27）ははっかしたのでこれを計時に使用することにした。

## entry.Sでのデバッグ出力

- 以下のコードは'4'を出力してストールする
- 1行ごとにこのコードを追加し、どこまで正常に動いているか確認する

```
ldr     x10, =UART_THR(0)
mov     w11, #52            // '4' : 任意のコード
strb    w11, [x10]
dsb     sy
isb
b     .
``

## console_init()以前のデバッグ出力

```c
*(volatile uint32_t *)0x01c28000 = 'M';

// メモリバリア
asm volatile("dsb sy" : : : "memory");
asm volatile("isb" : : : "memory");
```

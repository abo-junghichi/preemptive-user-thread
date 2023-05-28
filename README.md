# preemptimve user threadの実装テスト
unix環境で用意されているgetcontext(3)ファミリーとsigaction(2)を用いることで、
タイマー割り込みによるCPUレジスタまるごとのコンテキストスイッチを
アセンブラを用いずに実装。
POSIX.1-2001に準拠した環境上であれば、
当C言語コードをコンパイルするだけで、動作するバイナリを得られる。

## テスト済み環境
x86 Linux-4.14

## 注意
getcontext(3)ファミリーはPOSIX.1-2008で廃止されている。
glibcではまだ保守されているが、dietlibcやmusl libcでは実装されていない。

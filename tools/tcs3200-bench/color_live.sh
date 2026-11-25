#!/bin/bash
ELF=build/tcs_test.elf
K=$(arm-none-eabi-nm $ELF | awk '$3=="g_class"{print "0x"$1}')
names=("KHONG CO VAT" "DO" "XANH LA" "XANH DUONG" "TRANG" "KHONG XAC DINH")
last=-1
echo "Dua giay vao cam bien (Ctrl+C de thoat)"
while true; do
  v=$(openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
      -c "init; halt; echo [format {CLS=%d} [read_memory $K 32 1]]; resume; shutdown" 2>&1 \
      | grep -o "CLS=[0-9]*" | cut -d= -f2)
  if [ -n "$v" ] && [ "$v" != "$last" ]; then
    echo "-> ${names[$v]}"
    last=$v
  fi
done

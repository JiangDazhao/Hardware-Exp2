onbreak {quit -f}
onerror {quit -f}

vsim -t 1ps -lib xil_defaultlib PS918106840727_opt

do {wave.do}

view wave
view structure
view signals

do {PS918106840727.udo}

run -all

quit -force

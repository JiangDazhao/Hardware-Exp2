onbreak {quit -f}
onerror {quit -f}

vsim -t 1ps -lib xil_defaultlib item3_opt

do {wave.do}

view wave
view structure
view signals

do {item3.udo}

run -all

quit -force

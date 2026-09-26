# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/ethernet_image_transfer_system/_ide/scripts/debugger_ethernet_image_transfer-default.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/ethernet_image_transfer_system/_ide/scripts/debugger_ethernet_image_transfer-default.tcl
# 
connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent DSDB 210014A5B25FA" && level==0 && jtag_device_ctx=="jsn-DSDB-210014A5B25FA-23727093-0"}
fpga -file /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/ethernet_image_transfer/_ide/bitstream/zynq_wrapper.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/zynq_wrapper/export/zynq_wrapper/hw/zynq_wrapper.xsa -mem-ranges [list {0x40000000 0xbfffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
source /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/ethernet_image_transfer/_ide/psinit/ps7_init.tcl
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "*A9*#0"}
dow /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/ethernet_image_transfer/Debug/ethernet_image_transfer.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "*A9*#0"}
con

# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/zynq_wrapper/platform.tcl
# 
# OR launch xsct and run below command.
# source /home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/zynq_wrapper/platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {zynq_wrapper}\
-hw {/home/alexey/programming/systemverilog/ethernet_image_transfer/fpga/zynq_wrapper.xsa}\
-fsbl-target {psu_cortexa53_0} -out {/home/alexey/programming/systemverilog/ethernet_image_transfer/fpga}

platform write
domain create -name {standalone_ps7_cortexa9_0} -display-name {standalone_ps7_cortexa9_0} -os {standalone} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {empty_application}
platform generate -domains 
platform active {zynq_wrapper}
domain active {zynq_fsbl}
domain active {standalone_ps7_cortexa9_0}
platform generate -quick
platform generate
domain active {zynq_fsbl}
bsp reload
bsp setlib -name lwip211 -ver 1.3
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains zynq_fsbl 
bsp write
domain active {standalone_ps7_cortexa9_0}
bsp reload
bsp setlib -name lwip211 -ver 1.3
bsp removelib -name lwip211
bsp write
platform generate -domains 
bsp setlib -name lwip211 -ver 1.3
bsp write
bsp reload
catch {bsp regenerate}
bsp write
platform generate -domains standalone_ps7_cortexa9_0 
bsp config lwip_dhcp "true"
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_ps7_cortexa9_0 
bsp reload
domain active {zynq_fsbl}
bsp reload
platform generate

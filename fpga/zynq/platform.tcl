# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct /home/alexey/programming/fpga/workspace/zynq/platform.tcl
# 
# OR launch xsct and run below command.
# source /home/alexey/programming/fpga/workspace/zynq/platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {zynq}\
-hw {/home/alexey/programming/fpga/workspace/zynq.xsa}\
-fsbl-target {psu_cortexa53_0} -out {/home/alexey/programming/fpga/workspace}

platform write
domain create -name {standalone_ps7_cortexa9_0} -display-name {standalone_ps7_cortexa9_0} -os {standalone} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {lwip_udp_perf_client}
platform generate -domains 
platform active {zynq}
domain active {zynq_fsbl}
domain active {standalone_ps7_cortexa9_0}
platform generate -quick

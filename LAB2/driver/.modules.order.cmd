cmd_/home/vuong/embedded_lab2/driver/modules.order := {   echo /home/vuong/embedded_lab2/driver/lab2_driver.ko; :; } | awk '!x[$$0]++' - > /home/vuong/embedded_lab2/driver/modules.order

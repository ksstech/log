# log
 ESP32 LOG module replacement
 Adds BSD syslog functionality to the module using ksstech/syslog repository.
 Automatically adds the calling task name as well as the MCU# (0/1)
 Messages are displayed on both the UART console (if present) as well as logged to  a server.
 Currently only UDP (port 514) supported but TCP (possibly with SSL/TLS) planned
 

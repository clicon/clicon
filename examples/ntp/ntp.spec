#ntp
name="datamodel"; # Name of syntax-tree
ntp("Network Time Protocol"){
    logging("Configure NTP message logging") <status:bool>;
    server[ipv4addr]("Configure NTP Server"){
       <ipv4addr:ipv4addr>("IPv4 address of peer");
    }
}
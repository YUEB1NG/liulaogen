#include "cast_network.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *good[]={"https://example.com","https://cast.example.com:443","http://192.168.1.5:8766","http://10.0.0.1","http://172.16.0.1","http://172.31.255.254"};
    const char *bad[]={"","http://example.com","http://127.0.0.1","http://169.254.0.1","http://172.32.0.1","http://172.15.0.1","http://192.169.1.1","http://192.168.1.256","http://192.168.001.1","http://192.168.1","http://192.168.1.1.evil","http://0x7f000001","https://user:pass@example.com","https://example.com/","https://example.com?q=x","https://example.com#x","https://example.com:0","https://example.com:65536","https://example.com:443/path","https://example.com:\n443","ftp://example.com","https://.example.com"};
    for(size_t i=0;i<sizeof(good)/sizeof(*good);i++) assert(cast_net_origin_valid(good[i]));
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) assert(!cast_net_origin_valid(bad[i]));
    cast_net_config_t c={.version=1,.ssid="wifi",.password="12345678",.origin="https://example.com"};
    assert(cast_net_config_valid(&c));c.password[7]=0;assert(!cast_net_config_valid(&c));c.password[0]=0;assert(cast_net_config_valid(&c));
    c.origin[0]=0;assert(cast_net_config_valid(&c)); /* Hotspot-only, HTTP still unconfigured. */
    strcpy(c.origin,"http://example.com");assert(!cast_net_config_valid(&c));
    strcpy(c.origin,"https://example.com");
    memset(c.ssid,'a',32);c.ssid[32]=0;assert(cast_net_config_valid(&c));c.ssid[32]='b';assert(!cast_net_config_valid(&c));
    c.ssid[32]=0;c.ssid[0]='\n';assert(!cast_net_config_valid(&c));
    puts("Network config: PASS (URL scheme, private IPv4 boundaries, credentials and byte limits)");
}

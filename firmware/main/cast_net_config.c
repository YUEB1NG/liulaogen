#include "cast_network.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

bool cast_net_origin_valid(const char *s) {
    if(!s || strlen(s)>160) return false;
    bool tls=!strncmp(s,"https://",8);
    if(!tls && strncmp(s,"http://",7)) return false;
    const char *host=s+(tls?8:7), *p=host;
    if(!*p) return false;
    for(;*p && *p!=':';p++) if(!isalnum((unsigned char)*p) && *p!='.' && *p!='-') return false;
    size_t n=(size_t)(p-host);
    if(!n || n>253 || host[0]=='.' || host[0]=='-' || host[n-1]=='.' || host[n-1]=='-') return false;
    if(*p==':') {
        char *end; const char *port=p+1;
        if(!isdigit((unsigned char)*port)) return false;
        unsigned long value=strtoul(port,&end,10);
        if(*end || value==0 || value>65535 || strlen(port)>5) return false;
    }
    if(tls) return true;
    /* Cleartext is explicitly limited to RFC1918 IPv4 for LAN development. */
    unsigned a[4]={0}; const char *q=host;
    for(unsigned i=0;i<4;i++) {
        const char *start=q;
        while(q<host+n && isdigit((unsigned char)*q)) { a[i]=a[i]*10+(unsigned)(*q++-'0'); if(a[i]>255) return false; }
        if(q==start || q-start>3 || (q-start>1 && *start=='0')) return false;
        if(i<3) { if(*q++!='.') return false; }
    }
    return q==host+n && (a[0]==10 || (a[0]==172 && a[1]>=16 && a[1]<=31) || (a[0]==192 && a[1]==168));
}
bool cast_net_config_valid(const cast_net_config_t *c) {
    if(!c || c->version!=1 || !memchr(c->ssid,0,sizeof(c->ssid)) ||
       !memchr(c->password,0,sizeof(c->password)) || !memchr(c->origin,0,sizeof(c->origin))) return false;
    size_t ss=strlen(c->ssid), pw=strlen(c->password);
    if(!ss)return !pw && cast_net_origin_valid(c->origin); /* Service may be entered before Wi-Fi. */
    if(ss>32 || (pw && (pw<8 || pw>63))) return false;
    for(size_t i=0;i<ss;i++) if((unsigned char)c->ssid[i]<32 || c->ssid[i]==127) return false;
    for(size_t i=0;i<pw;i++) if((unsigned char)c->password[i]<32 || (unsigned char)c->password[i]>126) return false;
    /* A phone hotspot can be saved before the update service is deployed. */
    return !c->origin[0] || cast_net_origin_valid(c->origin);
}

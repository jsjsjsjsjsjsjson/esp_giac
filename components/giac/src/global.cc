/* -*- compile-command: "g++-3.4 -I.. -g -c global.cc  -DHAVE_CONFIG_H -DIN_GIAC" -*- */
#ifdef WIN32
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef __MINGW_H
#include <windows.h>
#endif
#endif

#include "giacPCH.h"
#if defined(EMCC) || defined(EMCC2)
#include <emscripten.h>
#endif

/*  
 *  Copyright (C) 2000,14 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef KHICAS
  extern "C" double millis(); //extern int time_shift;
#endif

using namespace std;
#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif
#if !defined GIAC_HAS_STO_38 && !defined NSPIRE && !defined FXCG 
#include <fstream>
#endif
#include "global.h"
// #include <time.h>
#if !defined BESTA_OS && !defined FXCG
#include <signal.h>
#endif
#include <math.h>
#ifndef WINDOWS
#include <stdint.h>   // for uintptr_t
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <string.h>
#include <stdexcept>
#include <algorithm>
#if !defined RTOS_THREADX
//#include <vector>
#endif
#if !defined BESTA_OS && !defined FXCG
#include <cerrno>
#endif
#include "gen.h"
#include "identificateur.h"
#include "symbolic.h"
#include "sym2poly.h"
#include "plot.h"
#include "rpn.h"
#include "prog.h"
#include "usual.h"
#include "tex.h"
#include "path.h"
#include "input_lexer.h"
#include "giacintl.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef _HAS_LIMITS
#include <limits>
#endif
#ifndef BESTA_OS
#ifdef WIN32
#if defined VISUALC
#if !defined FREERTOS
#include <chrono>
#include <thread>
#endif
#else
#if !defined(GNUWINCE) && !defined(__MINGW_H)
#include <sys/cygwin.h>
#endif
#if !defined(GNUWINCE) 
#include <windows.h>
#endif // ndef gnuwince
#endif // ndef visualc
#endif // win32
#endif // ndef bestaos

#ifdef HAVE_LIBFLTK
#include <FL/fl_ask.H>
#endif

#if defined VISUALC && defined GIAC_HAS_STO_38 && !defined BESTA_OS && !defined RTOS_THREADX && !defined FREERTOS 
#include <Windows.h>
#endif 

#ifdef BESTA_OS
#include <stdlib.h>
#endif // besta_os

#include <stdio.h>
#include <stdarg.h>

#ifdef QUICKJS
#include "qjsgiac.h"
string js_vars;
void update_js_vars(){
  const char VARS[]="function update_js_vars(){let res=''; for(var b in globalThis) { let prop=globalThis[b]; if (globalThis.hasOwnProperty(b)) res+=b+' ';} return res;}; update_js_vars()";
  char * names=js_ck_eval(VARS,&global_js_context);
  if (names){
    js_vars=names;
    free(names);
  }
}
int js_token(const char * buf){
  return js_token(js_vars.c_str(),buf);
}
#else // QUICKJS
void update_js_vars(){}
int js_token(const char * buf){
  return 0;
}
#endif
int js_token(const char * list,const char * buf){
  int bufl=strlen(buf);
  for (const char * p=list;*p;){
    if (p[0]=='\'')
      ++p;
    if (strncmp(p,buf,bufl)==0 && 
	(p[bufl]==0 || p[bufl]==' ' || p[bufl]=='\'')){
      return (p[bufl]=='\'')?3:2;
    }
    // skip to next keyword in p
    for (;*p;++p){
      if (*p==' '){
	++p; break;
      }
    }
  }
  return 0;
}

#ifdef HAVE_LIBMICROPYTHON
std::string & python_console(){
  static std::string * ptr=0;
  if (!ptr)
    ptr=new string;
  return *ptr;
}
#endif
bool freezeturtle=false;

#if defined(FIR)
extern "C" int firvsprintf(char*,const char*, va_list);
#endif

#ifdef FXCG
extern "C" int KeyPressed( void );
#endif

#if defined KHICAS || defined SDL_KHICAS
#include "kdisplay.h"
#endif

#ifdef NSPIRE_NEWLIB
#include <libndls.h>
#endif

#if defined NUMWORKS  && defined DEVICE
extern "C" const char * extapp_fileRead(const char * filename, size_t *len, int storage);
extern "C"  bool extapp_erasesector(void *);
extern "C"  bool extapp_writememory(unsigned char * dest,const unsigned char * data,size_t length);
#endif

#ifdef NUMWORKS
size_t pythonjs_stack_size=30*1024,
#ifdef DEVICE
  pythonjs_heap_size=_heap_size/2.4;
#else
  pythonjs_heap_size=40*1024;
#endif // DEVICE
#else // NUMWORKS
  size_t pythonjs_stack_size=128*1024,pythonjs_heap_size=(2*1024-256-64)*1024;
#endif
void * bf_ctx_ptr=0;
size_t bf_global_prec=128; // global precision for BF

int sprintf512(char * s, const char * format, ...){
  int z;
  va_list ap;
  va_start(ap,format);
#if defined(FIR) && !defined(FIR_LINUX)
  z = firvsnprintf(s, 512, format, ap);
#else
  z = vsnprintf(s, 512, format, ap);
#endif
  va_end(ap);
  return z;
}

int my_sprintf(char * s, const char * format, ...){
  int z;
  va_list ap;
  va_start(ap,format);
#if defined(FIR) && !defined(FIR_LINUX)
  z = firvsprintf(s, format, ap);
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  z = vsprintf(s, format, ap);
  // z = vsnprintf(s, RAND_MAX,format, ap);
#pragma clang diagnostic pop  
#endif
  va_end(ap);
  return z;
}

int ctrl_c_interrupted(int exception){
  if (!giac::ctrl_c && !giac::interrupted)
    return 0;
  giac::ctrl_c=giac::interrupted=0;
#ifndef NO_STDEXCEPT
  if (exception)
    giac::setsizeerr("Interrupted");
#endif
  return 1;
}

void console_print(const char * s){
  *logptr(giac::python_contextptr) << s;
}

const char * console_prompt(const char * s){
  static string S;
  giac::gen g=giac::_input(giac::string2gen(s?s:"?",false),giac::python_contextptr);
  S=g.print(giac::python_contextptr);
  return S.c_str();
}

#if !defined USE_GMP_REPLACEMENTS && !defined GIAC_HAS_STO_38

  /*********************************************************************
   * Filename:   sha256.c/.h
   * Author:     Brad Conte (brad AT bradconte.com)
   * Copyright:
   * Disclaimer: This code is presented "as is" without any guarantees.
   * Details:    Implementation of the SHA-256 hashing algorithm.
              SHA-256 is one of the three algorithms in the SHA2
              specification. The others, SHA-384 and SHA-512, are not
              offered in this implementation.
              Algorithm specification can be found here:
	      * http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf
              This implementation uses little endian byte order.
  *********************************************************************/
    
  /****************************** MACROS ******************************/
#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

  /**************************** VARIABLES *****************************/
  static const WORD32 k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
  };

  /*********************** FUNCTION DEFINITIONS ***********************/
  void giac_sha256_transform(SHA256_CTX *ctx, const BYTE data[])
  {
    WORD32 a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
      m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for ( ; i < 64; ++i)
      m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
      t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
      t2 = EP0(a) + MAJ(a,b,c);
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
  }

  void giac_sha256_init(SHA256_CTX *ctx)
  {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
  }

  void giac_sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len)
  {
    WORD32 i;

    for (i = 0; i < len; ++i) {
      ctx->data[ctx->datalen] = data[i];
      ctx->datalen++;
      if (ctx->datalen == 64) {
	giac_sha256_transform(ctx, ctx->data);
	ctx->bitlen += 512;
	ctx->datalen = 0;
      }
    }
  }

  void giac_sha256_final(SHA256_CTX *ctx, BYTE hash[])
  {
    WORD32 i;

    i = ctx->datalen;

    // Pad whatever data is left in the buffer.
    if (ctx->datalen < 56) {
      ctx->data[i++] = 0x80;
      while (i < 56)
	ctx->data[i++] = 0x00;
    }
    else {
      ctx->data[i++] = 0x80;
      while (i < 64)
	ctx->data[i++] = 0x00;
      giac_sha256_transform(ctx, ctx->data);
      memset(ctx->data, 0, 56);
    }

    // Append to the padding the total message's length in bits and transform.
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    giac_sha256_transform(ctx, ctx->data);

    // Since this implementation uses little endian byte ordering and SHA uses big endian,
    // reverse all the bytes when copying the final state to the output hash.
    for (i = 0; i < 4; ++i) {
      hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
      hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
      hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
      hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
      hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
      hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
      hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
      hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
  }
  /* END OF SHA256 */

  
#endif

// support for tar archive in flash on the numworks
char * buf64k=0; // we only have 64k of RAM buffer on the Numworks
const size_t buflen=(1<<16);
#if defined NUMWORKS_SLOTB 
int numworks_maxtarsize=0x200000-0x10000;
#else
#ifdef NUMWORKS_SLOTAB
int numworks_maxtarsize=0x60000;
#else
int numworks_maxtarsize=0x600000-0x10000;
#endif
#endif
size_t tar_first_modified_offset=0; // set to non 0 if tar data comes from Numworks


// erase sector containing address if required
// returns true if erased, false otherwise 
// if false is returned, it means that [address..end of sector] contains 0xff
// i.e. the remaining part of this sector is ready to write without erasing
void erase_sector(const char * buf){ 
#if defined NUMWORKS && defined DEVICE
  extapp_erasesector((void *)buf);
#else
  char * nxt=(char *) ((((size_t) buf)/buflen +1)*buflen);
  char * start=nxt-buflen;
  for (int i=0;i<buflen;++i)
    start[i]=0xff;
#endif
}

#if defined NUMWORKS && defined DEVICE
void WriteMemory(char * target,const char * src,size_t length){
  extapp_writememory((unsigned char *)target,(unsigned char*)src,length);
}

#else
void WriteMemory(char * target,const char * src,size_t length){
  memcpy(target,src,length);
}
#endif


#if 0
bool write_memory(char * target,const char * src,size_t length_){
  //if (length % 256) return false;
  size_t length=((255+length_)/256)*256;
  char * nxt=(char *) ((((size_t) target)/buflen +1)*buflen);
  size_t delta=length;
  if (delta>nxt-target)
    delta=nxt-target;
  // first copy current sector in buf64k before erasing
  char * prev=(char *)nxt-buflen;
  memcpy(buf64k,prev,buflen);
  memcpy(buf64k+(target-prev),src,delta);
  erase_sector(target);
  WriteMemory(prev,buf64k,buflen);
  length -= delta;
  src += delta;
  target += delta;
  while (length>0){
    memcpy(buf64k,target,buflen);
    delta=length;
    if (delta>buflen) 
      delta=buflen;
    memcpy(buf64k,src,delta);
    erase_sector(target);
    WriteMemory(target,buf64k,buflen);
    length -= delta;
    src += delta;
    target += delta;      
  }
}
#endif

// TAR: tar file format support
int tar_filesize(int s){
  int h=(s/512+1)*512;
  if (s%512)
    h +=512;
  return h;
}

// adapted from tarballjs https://github.com/ankitrohatgi/tarballjs
string giac_readString(const char * buffer,size_t str_offset, size_t size) {
  int i = 0;
  string rtnStr = "";
  while(i<size) {
    unsigned char ch=buffer[str_offset+i];
    if (ch<32 || ch>=128) break;
    rtnStr += ch;
    i++;
  }
  return rtnStr;
}
string giac_readFileName(const char * buffer,size_t header_offset) {
  return giac_readString(buffer,header_offset, 100);
}
string giac_readFileType(const char * buffer,size_t header_offset) {
  // offset: 156
  const char typeStr = buffer[header_offset+156];
  if (typeStr == '0')
    return "file";
  if (typeStr == '5')
    return "directory";
  return string(1,typeStr);
}
int giac_readFileSize(const char * buffer,size_t header_offset) {
  // offset: 124
  const char * szView = buffer+ header_offset+124;
  int res=0;
  for (int i = 0; i < 11; i++) {
    char tmp=szView[i];
    if (tmp<'0' || tmp>'9') return -1; // invalid file size
    res *= 8;
    res += (tmp-'0');
  }
  return res;
}

int giac_readMode(const char * buffer,size_t header_offset) {
  // offset: 100
  const char * szView = buffer+ header_offset+100;
  int res=0;
  for (int i = 0; i < 7; i++) {
    char tmp=szView[i];
    if (tmp==' ')
      return res;
    if (tmp<'0' || tmp>'9') 
      return -1; // invalid file size
    res *= 10;
    res += (tmp-'0');
  }
  return res;
}

void tar_clear(char * buffer){
  for (int i=0;i<1024;++i)
    buffer[i]=0;
}

std::vector<fileinfo_t> tar_fileinfo(const char * buffer,size_t byteLength){
  vector<fileinfo_t> fileInfo;
  if (!buffer) return fileInfo;
  size_t offset=0,file_size=0;       
  string file_name = "";
  string file_type = "";
  while (byteLength==0 || offset<byteLength-512){
    file_name = giac_readFileName(buffer,offset); // file name
    if (file_name.size() == 0) 
      break;
    file_type = giac_readFileType(buffer,offset);
    file_size = giac_readFileSize(buffer,offset);
    int mode = giac_readMode(buffer,offset);
    if (file_size<0)
      break;
    //console.log(offset,file_name,file_size);
    fileinfo_t tmp={file_name,file_type,file_size,offset,mode};
    fileInfo.push_back(tmp);
    offset += tar_filesize(file_size);
  }
  //console.log('fileinfo',offset,dohtml);
  return fileInfo;
}

size_t tar_totalsize(const char * buffer,size_t byteLength){
  std::vector<fileinfo_t> f=tar_fileinfo(buffer,byteLength);
  if (f.empty()) return 0;
  fileinfo_t i=f[f.size()-1];
  size_t offset=i.header_offset+tar_filesize(i.size);
  return offset;
}

std::string leftpad(const string & s,size_t targetLength) {
  if (targetLength<=s.size())
    return s;
  string add(targetLength-s.size(),'0');
  return add+s;
}
  
void tar_writestring(char * buffer,const string & str, size_t offset, size_t size) {
  for (size_t i = 0; i < size; i++) {
    if (i < str.size()) 
      buffer[i+offset] = str[i];
    else 
      buffer[i+offset] = 0;
  }
}

std::string toString8(longlong chksum){
  if (chksum<0)
    return "-"+toString8(-chksum);
  if (chksum==0)
    return "0";
  string res;
  for (;chksum;chksum/=8){
    res = string(1,'0'+(chksum % 8))+res;
  }
  return res;
}

ulonglong fromstring8(const char * ptr){
  ulonglong res=0; char ch;
  for (;(ch=*ptr);++ptr){
    if (ch==' ')
      return res;
    if (ch<'0' || ch>'8')
      return -1;
    res *= 8;
    res += ch-'0';
  }
  return res;
}

void tar_writechecksum(char * buffer,size_t header_offset) {
  // offset: 148
  tar_writestring(buffer,"        ", header_offset+148, 8); // first fill with spaces
  // add up header bytes
  int chksum = 0;
  for (int i = 0; i < 512; i++) {
    chksum += buffer[header_offset+i];
  }
  tar_writestring(buffer,leftpad(toString8(chksum),6), header_offset+148, 8);
  tar_writestring(buffer," ",header_offset+155,1); // add space inside chksum field
}

void tar_fillheader(char * buffer,size_t offset,int exec=0){
  int uid = 501;
  int gid = 20;
  string mode = exec?"755":"644"; 
#if !defined HAVE_NO_SYS_TIMES_H && defined HAVE_SYS_TIME_H
  struct timeval t;
  gettimeofday(&t, NULL);
  longlong mtime=t.tv_sec;
#else
  longlong mtime=(2021LL-1970)*24*365.2425*3600;
#ifdef KHICAS
  mtime = millis()/1000;
#endif
#endif
  string user = "user";
  string group = "group";

  tar_writestring(buffer,leftpad(mode,7)+" ", offset+100, 8);  
  tar_writestring(buffer,leftpad(toString8(uid),6)+" ",offset+108,8);
  tar_writestring(buffer,leftpad(toString8(gid),6)+" ",offset+116,8);
  tar_writestring(buffer,leftpad(toString8(mtime),11)+" ",offset+136,12);

  //UI.tar_writestring(buffer,"ustar", offset+257,6); // magic string
  //UI.tar_writestring(buffer,"00", offset+263,2); // magic version
  tar_writestring(buffer,"ustar  ", offset+257,8);
    
  tar_writestring(buffer,user, offset+265,32); // user
  tar_writestring(buffer,group, offset+297,32); //group
  tar_writestring(buffer,"000000 ",offset+329,7); //devmajor
  tar_writestring(buffer,"000000 ",offset+337,7); //devmajor
  tar_writechecksum(buffer,offset);
}

// flash version
int flash_adddata(const char * buffer_,const char * filename,const char * data,size_t datasize,int exec){
  vector<fileinfo_t> finfo=tar_fileinfo(buffer_,numworks_maxtarsize);
  size_t s=finfo.size(),offset=0;
  if (s){
    fileinfo_t last=finfo[s-1];
    offset=last.header_offset;
    offset += tar_filesize(last.size);
  }
  if (offset+1024+datasize>numworks_maxtarsize) return 0;
  buffer_ += offset;
  char * nxt=(char *) ((((size_t) buffer_)/buflen +1)*buflen);
  char * prev=nxt-buflen;
  size_t pos=buffer_-prev;
  for (int i=0;i<buflen;++i)
    buf64k[i]=0xff;
  memcpy(buf64k,prev,pos);
  char * buffer=buf64k+pos; // point to header
  // fill with 0
  for (int i=0;i<512;++i)
    buffer[i]=0;
  tar_writestring(buffer,filename,0,100); // filename
  tar_writestring(buffer,leftpad(toString8(datasize),11)+" ",124,12);  // filesize
  tar_writestring(buffer,"0",156,1); // file type
  tar_fillheader(buffer,0,exec);
  // add data
  buffer += 512;
  pos += 512;
  size_t length=datasize;
  if (length>buflen-pos)
    length=buflen-pos;
  memcpy(buffer,data,length);
  buffer += length;
  for (;(size_t) buffer % 512;++buffer)
    *buffer=0;
  erase_sector(prev);
  WriteMemory(prev,buf64k,buflen);
  datasize -= length;
  data += length;
  prev += buflen;
  while (datasize>0){
    // copy remaining data
    length=datasize<buflen?datasize:buflen;
    memcpy(buf64k,data,length);
    for (int i=length;i<buflen;++i)
      buf64k[i]=0xff;
    erase_sector(prev);
    WriteMemory(prev,buf64k,buflen);
    datasize -= length;
  }
  return 1;
}

// RAM version
int tar_adddata(char * & buffer,size_t * buffersizeptr,const char * filename,const char * data,size_t datasize,int exec){
  size_t buffersize=buffersizeptr?*buffersizeptr:0;
  vector<fileinfo_t> finfo=tar_fileinfo(buffer,buffersize);
  size_t s=finfo.size(),offset=0;
  if (s){
    fileinfo_t last=finfo[s-1];
    offset=last.header_offset;
    offset += tar_filesize(last.size);
  }
  buffersize=offset;
  size_t newsize=offset+1024+datasize;
  newsize=10240*((newsize+10239)/10240);
  if (newsize>numworks_maxtarsize) return 0;
  // console.log(buffer.byteLength,newsize);
  // resize buffer
  if (buffersize<newsize){
    char * newbuf=(char *)malloc(newsize);
    // char * newbuf=new char[newsize];
    memcpy(newbuf,buffer,buffersize);
    // delete [] buffer;
    free(buffer);
    buffersize=newsize;
    if (buffersizeptr) *buffersizeptr=buffersize;
    buffer=newbuf;
  }
  // console.log(buffer.byteLength,newsize);
  // fill header with 0
  for (int i=0;i<1024;++i)
    buffer[offset+i]=0;
  tar_writestring(buffer,filename,offset,100); // filename
  tar_writestring(buffer,leftpad(toString8(datasize),11)+" ",offset+124,12);  // filesize
  tar_writestring(buffer,"0",offset+156,1); // file type
  tar_fillheader(buffer,offset,exec);
  // copy data 
  for (size_t i=0;i<datasize;++i)
    buffer[offset+512+i]=data[i];
  return 1;
}

// flash version
int flash_addfile(const char * buffer,const char * filename){
#if defined NUMWORKS  && defined DEVICE
  size_t len;
  const char * ch=extapp_fileRead(filename,&len,1); // read file in ram
  return flash_adddata(buffer,filename,ch,len,0); // copy in flash
#else
  vector<char> data;
  FILE * f = fopen(filename,"rb");
  while (1){
    char ch=fgetc(f);
    if (feof(f))
      break;
    data.push_back(ch);
  }
  fclose(f);
  int exec=1;
  for (int i=0;filename[i];++i){
    if (filename[i]=='.')
      exec=0;
  }
  string fname=filename;
  for (int i=0;filename[i];++i){
    if (filename[i]=='/')
      fname=filename+i+1;
  }
  return flash_adddata(buffer,fname.c_str(),&data.front(),data.size(),exec);
#endif
}

// RAM version
int tar_addfile(char * & buffer,const char * filename,size_t * buffersizeptr){
  FILE * f = fopen(filename,"rb");
  vector<char> data;
  while (1){
    char ch=fgetc(f);
    if (feof(f))
      break;
    data.push_back(ch);
  }
  fclose(f);
  int exec=1;
  for (int i=0;filename[i];++i){
    if (filename[i]=='.')
      exec=0;
  }
  string fname=giac::remove_path(filename);
  return tar_adddata(buffer,buffersizeptr,fname.c_str(),&data.front(),data.size(),exec);
}

int tar_savefile(char * buffer,const char * filename){
  vector<fileinfo_t> finfo=tar_fileinfo(buffer,0);
  int s=finfo.size();
  if (s==0) return 0;
  fileinfo_t info;
  for (int i=0;i<s;++i){
    info=finfo[i];
    if (info.filename==filename)
      break;
  }
  if (info.filename!=filename) return 0;
  size_t target=info.header_offset+512;
  size_t size=info.size;
  FILE * f=fopen(filename,"wb");
  if (!f) return 0;
  fwrite(buffer+target,size,1,f);
  fclose(f);
  return 1;
}

#if 0
// flash version
// mark_only==1 mark for erase, ==2 undelete
// use mark_only==0 if and only if you need some space in flash
// otherwise it's safer to empty the trash from a PC
int flash_removefile(const char * buffer,const char * filename,size_t * tar_first_modif_offsetptr,int mark_only){
  vector<fileinfo_t> finfo=tar_fileinfo(buffer,0);
  int s=finfo.size();
  if (s==0) return 0;
  fileinfo_t info;
  for (int i=0;i<s;++i){
    info=finfo[i];
    if (info.filename==filename)
      break;
  }
  if (info.filename!=filename) return 0;
  // new code, mark file as non readable
  size_t target=info.header_offset;
  if (tar_first_modif_offsetptr && target<*tar_first_modif_offsetptr)
    *tar_first_modif_offsetptr=target;
  // mark the file as non readable
  if (mark_only){
    char headbuf[512];
    memcpy(headbuf,buffer+target,512);
    if (mark_only==1)
      headbuf[104] = '0'+((headbuf[104]-'0') &3);
    else
      headbuf[104] = '0'+((headbuf[104]-'0') |4);
    tar_writechecksum(headbuf,0);
    // recompute chksum
    write_memory((char *)buffer+target,headbuf,512);
    return 1;
  }
  // really erase, move info.header_offset+info.size+512 to info.header_offset
  size_t src=target+tar_filesize(info.size);
  fileinfo_t infoend=finfo[s-1];
  size_t end=infoend.header_offset+tar_filesize(infoend.size);
  write_memory((char *)buffer+target,buffer+src,end-src);
  // clear space after new end
  // for (;target<end;++target)  buffer[target]=0;
  return 1;
}
#endif

const char * tar_loadfile(const char * buffer,const char * filename,size_t * len){
  vector<fileinfo_t> finfo=tar_fileinfo(buffer,0);
  int s=finfo.size();
  for (int i=0;i<s;++i){
    const fileinfo_t & f=finfo[i];
    if (f.filename==filename){
      if (len)
	*len=f.size;
      else
        ;//return "";
      return buffer+f.header_offset+512;
    }
  }
  return 0;
}

bool match(const char * filename,const char * extension){
  if (!extension)
    return true;
  int el=strlen(extension);
  int fl=strlen(filename);
  if (el>=fl) return false;
  int i=fl-el,j=0;
  for (;j<el;++i,++j){
    if (filename[i]!=extension[j])
      return false;
  }
  return true;
}

int tar_filebrowser(const char * buf,const char ** filenames,int maxrecords,const char * extension){
  static vector<fileinfo_t> finfo;
  finfo=tar_fileinfo(buf,0);
  int s=finfo.size();
  if (s==0) return 0;
  int j=0;
  for (int i=0;i<s;++i){
    const fileinfo_t & f=finfo[i];
    if (match(f.filename.c_str(),extension)){
      filenames[j]=f.filename.c_str();
      ++j;
      if (j==maxrecords)
	return j;
    }
  }
  return j;
}

// RAM version
int tar_removefile(char * buffer,const char * filename,size_t * tar_first_modif_offsetptr){
  vector<fileinfo_t> finfo=tar_fileinfo(buffer,0);
  int s=finfo.size();
  if (s==0) return 0;
  fileinfo_t info;
  for (int i=0;i<s;++i){
    info=finfo[i];
    if (info.filename==filename)
      break;
  }
  if (info.filename!=filename) return 0;
  // move info.header_offset+info.size+512 to info.header_offset
  size_t target=info.header_offset;
  if (tar_first_modif_offsetptr && target<*tar_first_modif_offsetptr)
    *tar_first_modif_offsetptr=target;
  size_t src=target+tar_filesize(info.size);
  fileinfo_t infoend=finfo[s-1];
  size_t end=infoend.header_offset+tar_filesize(infoend.size);
  // memcpy would be faster, but I'm unsure it is safe here
  for (;src<end;++src,++target) 
    buffer[target]=buffer[src];
  for (;target<end;++target) // clear space after new end
    buffer[target]=0;
  return 1;
}

bool tar_nxt_readable(const vector<fileinfo_t> & finfo,int cur,fileinfo_t & f){
  int s=finfo.size();
  for (int i=cur;i<s;++i){
    f=finfo[i];
    int droit=f.mode/100 ;
    if ( (droit & 4)==4)
      return true;
  }
  return false;
}

bool same_offset_size (const fileinfo_t & a,const fileinfo_t &b){
  return a.header_offset==b.header_offset && a.size==b.size;
}

int flash_synchronize(const char * buffer,const vector<fileinfo_t> & finfo,size_t * tar_first_modif_offsetptr){
  vector<fileinfo_t> oinfo=tar_fileinfo(buffer,0);
  int s=finfo.size();
  if (oinfo.size()!=finfo.size())
    return 0;
  for (int i=0;i<s;++i){
    fileinfo_t f=finfo[i],o=oinfo[i];
    if (!same_offset_size(f,o))
      return 0;
    if (f.mode==o.mode && f.filename==o.filename)
      continue;
    if (tar_first_modif_offsetptr && *tar_first_modif_offsetptr>f.header_offset)
      *tar_first_modif_offsetptr=f.header_offset;
    // copy current sector 
    size_t sector_begin=(f.header_offset/buflen)*buflen,sector_end=sector_begin+buflen;
    memcpy(buf64k,buffer+sector_begin,buflen);
    // modify all records in this sector
    for (;i<s;++i){
      f=finfo[i];
      size_t sector_pos=f.header_offset-sector_begin;
      if (sector_pos>=buflen){
	break;
      }
      char * headbuf=buf64k+sector_pos;
      strcpy(headbuf,f.filename.c_str());
      if ( (f.mode/100 & 4) ==0)
	headbuf[104] = '0'+((headbuf[104]-'0') &3);
      else
	headbuf[104] = '0'+((headbuf[104]-'0') |4);
      tar_writechecksum(headbuf,0);
    }
    erase_sector(buffer+sector_begin);
    WriteMemory((char *)buffer+sector_begin,buf64k,buflen);
  } 
  return 1;
}

int flash_emptytrash(const char * buffer,const vector<fileinfo_t> & finfo,size_t * tar_first_modif_offsetptr){
  size_t flash_end=0x90800000LL-buflen,flash_begin=0x90200000LL;
  int s=finfo.size();
  if (s==0) return 0;
  // find 1st offset marked non readable
  int i; // record position
  fileinfo_t f,fnxt;
  for (i=0;i<s;++i){
    f=finfo[i];
    int droit=f.mode/100 ;
    if ( (droit & 4)==0){
      break;
    }
  }
  if (i==s) return 0;
  // if (!tar_nxt_readable(finfo,i,fnxt)) return 0;
  if (tar_first_modif_offsetptr && *tar_first_modif_offsetptr>f.header_offset)
    *tar_first_modif_offsetptr=f.header_offset;
  // find current sector
  size_t sector_begin=(f.header_offset/buflen)*buflen,sector_end=sector_begin+buflen;
  memcpy(buf64k,buffer+sector_begin,buflen);
  size_t sector_pos=f.header_offset-sector_begin; // in [0,buflen[
  int nwrite=0;
  for (;i<s;++nwrite){
    // buf64k[0..sector_pos[ is ok
    int nxti=i;
    for (++nxti;nxti<s;++nxti){
      fnxt=finfo[nxti];
      int droit=fnxt.mode /100;
      if ( (droit & 4)==4){
	break;
      }
    }
    if (nxti==s) break;
    size_t src=fnxt.header_offset;
    // find nxt offset marked as non readable
    for (++nxti;nxti<s;++nxti){
      f=finfo[nxti];
      int droit=f.mode /100;
      if ( (droit & 4)==0){
	break;
      }
    }
    size_t length = f.header_offset-src; 
    if (nxti==s)
      length += tar_filesize(f.size);
    // total number of bytes to be copied 
    while (length>0){
      size_t nbytes=length;
      if (length>buflen-sector_pos)
	nbytes=buflen-sector_pos;
      memcpy(buf64k+sector_pos,buffer+src,nbytes);
      sector_pos += nbytes;
      for (int j=sector_pos;j<buflen;++j)
	buf64k[j]=0xff;
      src += nbytes;
      length -= nbytes; 
      if (sector_pos==buflen){
	// erase sector, transfert buf64k, copy next sector in buf64k
	erase_sector((char *)buffer+sector_begin);
	WriteMemory((char *)buffer+sector_begin,buf64k,buflen);
	sector_begin += buflen;
	if (sector_begin>flash_end-flash_begin)
	  return 1;
	sector_end += buflen;
	sector_pos = 0;
	memcpy(buf64k,buffer+sector_begin,buflen);
      }
    }
    i=nxti;
  }
  if (sector_pos>0){
    erase_sector((char *)buffer+sector_begin);
    for (int j=sector_pos;j<buflen;++j)
      buf64k[j]=0xff;
    WriteMemory((char *)buffer+sector_begin,buf64k,buflen);
  }
  return nwrite;
}

int flash_emptytrash(const char * buffer,size_t * tar_first_modif_offsetptr){
  vector<fileinfo_t> finfo=tar_fileinfo(buffer,0);
  return flash_emptytrash(buffer,finfo,tar_first_modif_offsetptr);
}

char * file_gettar(const char * filename){
  FILE * f=fopen(filename,"rb");
  if (!f) return 0;
  vector<char> res;
  while (1){
    char ch=fgetc(f);
    if (feof(f))
      break;
    res.push_back(ch);
  }
  fclose(f);
  size_t size=res.size();
  size_t bufsize=65536*((size+65535)/65536);
  char * buffer=(char *)malloc(bufsize);
  memcpy(buffer,&res.front(),size);
  return buffer;
}

char * file_gettar_aligned(const char * filename,char * & freeptr){
  size_t size=numworks_maxtarsize;
  size_t bufsize=buflen*((size+(buflen-1))/buflen);
  char * buffer=(char *)malloc(bufsize+2*buflen);
  freeptr=buffer;
  // align buffer
  buffer=(char *) ((((size_t) buffer)/buflen +1)*buflen);
  FILE * f=fopen(filename,"rb");
  if (!f){
    for (size_t i=0;i<size;++i)
      buffer[i]=0;
    return buffer;
  }
  vector<char> res;
  while (1){
    char ch=fgetc(f);
    if (feof(f))
      break;
    res.push_back(ch);
  }
  fclose(f);
  size=res.size();
  if (size>numworks_maxtarsize)
    size=numworks_maxtarsize;
  memcpy(buffer,&res.front(),size);
  return buffer;
}


int file_savetar(const char * filename,char * buffer,size_t buffersize){
  size_t l=tar_totalsize(buffer,buffersize);
  if (l==0) return 0;
  FILE * f=fopen(filename,"wb");
  if (!f) return 0;
  fwrite(buffer,l,1,f);
  char buf[1024];
  for (int i=0;i<1024;++i)
    buf[i]=0;
  fwrite(buf,1024,1,f);
  fclose(f);
  return 1;
}
#if !defined KHICAS && !defined SDL_KHICAS && !defined USE_GMP_REPLACEMENTS && !defined GIAC_HAS_STO_38// 

#ifdef HAVE_LIBDFU
extern "C" { 
#include "dfu_lib.h"
}
#endif

// Numworks calculator
int dfu_exec(const char * s_){
  CERR << s_ << "\n";
#if 0 // def HAVE_LIBDFU
  std::istringstream ss(s_);
  std::string arg;
  std::vector<std::string> ls;
  std::vector<char*> v;
  while (ss >> arg)
    {
      ls.push_back(arg); 
      v.push_back(const_cast<char*>(ls.back().c_str()));
    }
  v.push_back(0);  // need terminating null pointer
  int res=dfu_main(v.size()-1,&v[0]);
  return res;
#else
#ifdef WIN32 
  string s(s_);
#ifdef __MINGW_H
  if (giac::is_file_available("c:\\xcaswin\\dfu-util.exe"))
    s="c:\\xcaswin\\"+s;
  // otherwise dfu-util should be in the path
#else
  if (giac::is_file_available("/cygdrive/c/xcas64/dfu-util.exe"))
    s="/cygdrive/c/xcas64/"+s;
  else
    s="./"+s;
#endif
  return system(s.c_str());
#else // WIN32
#ifdef __APPLE__
  string s(s_);
  s="/Applications/usr/bin/"+s;
  if (giac::is_file_available(s.c_str()))
    return giac::system_no_deprecation(s.c_str());
  s=s_; s="/opt/homebrew/bin/"+s;
  return giac::system_no_deprecation(s.c_str());
#else
  return system(s_);
#endif
#endif // WIN32
#endif 
}

const int dfupos=15; // position of ...-a0 or -a1 in dfu command

bool dfu_get_scriptstore_addr(size_t & start,size_t & taille,char & altdfu){
  // first try multi-boot
  const char * slots[]={"0x90000000","0x90180000","0x90400000"};
  const char * slots1[]={"0x90010000","0x90190000","0x90410000"};
  const char * slots2[]={"0x90020000","0x90190000","0x90420000"};
  unsigned char r[32];
  altdfu='0';
  for (int j=0;j<sizeof(slots)/sizeof(char *);++j){
    unlink("__platf");
    char cmd[256];
    strcpy(cmd,"dfu-util -i0 -a0 -s ");
    strcat(cmd,slots[j]);
    strcat(cmd,":0x20:force -U __platf");
    if (dfu_exec(cmd)){
      unlink("__platf");
      strcpy(cmd,"dfu-util -i0 -a1 -s ");
      strcat(cmd,slots[j]);
      strcat(cmd,":0x20:force -U __platf");
      if (dfu_exec(cmd))
        return false;
      altdfu='1';
    }
    FILE * f=fopen("__platf","r");
    if (!f){ return false; }
    int i=fread(r,1,32,f);
    fclose(f);
    if (i!=32)
      return false;
    // check valid slot: for f0 0d c0 de at offset 8, 9, a, b
    bool externalinfo=r[8]==0xf0 && r[9]==0x0d && r[10]==0xc0 && r[11]==0xde;
    if (!externalinfo)
      break;
    unlink("__platf");
    strcpy(cmd,"dfu-util -i0 -a0 -s ");
    cmd[dfupos]=altdfu;
    strcat(cmd,slots1[j]);
    strcat(cmd,":0x20:force -U __platf");
    if (dfu_exec(cmd))
      return false;
    f=fopen("__platf","r");
    if (!f){ return false; }
    i=fread(r,1,32,f);
    fclose(f);
    if (i!=32)
      return false;
    if (r[0]!=0xfe || r[1]!=0xed || r[2]!=0xc0 || r[3]!=0xde){
      unlink("__platf");
      strcpy(cmd,"dfu-util -i0 -a0 -s ");
      cmd[dfupos]=altdfu;
      strcat(cmd,slots2[j]);
      strcat(cmd,":0x20:force -U __platf");
      if (dfu_exec(cmd))
        return false;
      f=fopen("__platf","r");
      if (!f){ return false; }
      i=fread(r,1,32,f);
      fclose(f);
      if (i!=32)
        return false;
    }
    start=((r[15]*256U+r[14])*256+r[13])*256+r[12];
    if (r[15]!=0x20 && r[15]!=0x24) // ram is at 0x20000000 (+256K)
      continue;
    taille=((r[19]*256U+r[18])*256+r[17])*256+r[16];
    unlink("__platf");   // check 4 bytes at start address
    string ds="dfu-util -i0 -a0 -s "+giac::print_INT_(start)+":0x4:force -U __platf";
    ds[dfupos]=altdfu;
    if (dfu_exec((ds).c_str()))
      continue;
    f=fopen("__platf","r");
    if (!f){ return false; }
    i=fread(r,1,4,f);
    fclose(f);
    if (i!=4)
      return false;
    if (r[0]==0xba && r[1]==0xdd && r[2]==0x0b && r[3]==0xee) // ba dd 0b ee begin of scriptstore
      return true;
  }
  // no valid slot, try without bootloader
  unlink("__platf");
  string ds="dfu-util -i0 -a0 -s 0x080001c4:0x20:force -U __platf";
  ds[dfupos]=altdfu;
  if (dfu_exec(ds.c_str()))
    return false;
  FILE * f=fopen("__platf","r");
  if (!f){ return false; }
  int i=fread(r,1,32,f);
  fclose(f);
  if (i!=32)
    return false;
  start=((r[23]*256U+r[22])*256+r[21])*256+r[20];
  taille=((r[27]*256U+r[26])*256+r[25])*256+r[24];
  // taille=(taille/32768)*32768; // do not care of end of scriptstore
  return true;
}

char dfu_alt(){
  size_t start,taille;
  char altdfu;
  if (!dfu_get_scriptstore_addr(start,taille,altdfu)) return ' ';
  return altdfu;
}

bool dfu_get_scriptstore(const char * fname){
  unlink(fname);
  size_t start,taille;
  char altdfu;
  if (!dfu_get_scriptstore_addr(start,taille,altdfu)) return false;
  string s="dfu-util -i0 -a0 -U "+string(fname)+" -s "+ giac::print_INT_(start)+":"+giac::print_INT_(taille)+":force";
  s[dfupos]=altdfu;
  return !dfu_exec(s.c_str());
}

bool dfu_send_scriptstore(const char * fname){
  size_t start,taille;
  char altdfu;
  if (!dfu_get_scriptstore_addr(start,taille,altdfu)) return false;
  string s="dfu-util -i0 -a0 -D "+string(fname)+" -s "+ giac::print_INT_(start)+":"+giac::print_INT_(taille)+":force";
  s[dfupos]=altdfu;
  return !dfu_exec(s.c_str());
} 

bool dfu_send_rescue(const char * fname){
  string s=string("dfu-util -i0 -a0 -s 0x20030000:force:leave -D ")+ fname;
  s[dfupos]=dfu_alt();
  return !dfu_exec(s.c_str());
}

char hex2char(int i){
  i &= 0xf;
  if (i>=0 && i<=9)
    return '0'+i;
  return 'A'+(i-10);
}

// send to 0x90000000+offset*0x10000
bool dfu_send_firmware(const char * fname,int offset){
  string s=string("dfu-util -i0 -a0 -s 0x90");
  s[dfupos]=dfu_alt();
  s += hex2char(offset/16);
  s += hex2char(offset);
  s += "0000 -D ";
  s += fname;
  return !dfu_exec(s.c_str());
}

bool dfu_send_apps(const char * fname){
  string s=string("dfu-util -i0 -a0 -s 0x90200000 -D ")+ fname;
  return !dfu_exec(s.c_str());
}

bool dfu_send_slotab(const char * fnamea,const char * fnameb1,const char * fnameb2){
  size_t start,taille; char altdfu;
  if (!dfu_get_scriptstore_addr(start,taille,altdfu))
    return false;
  string s;
  if (fnamea){
    s=string("dfu-util -i0 -a0 -s 0x90260000 -D ")+ fnamea;
    if (dfu_exec(s.c_str()))
      return false;
  }
  s=string("dfu-util -i0 -a0 -s 0x90400000 -D ")+ (start>=0x24000000?fnameb2:fnameb1);
  return !dfu_exec(s.c_str());
}

bool dfu_get_epsilon_internal(const char * fname){
  unlink(fname);
  string s=string("dfu-util -i0 -a0 -s 0x08000000:0x8000:force -U ")+ fname;
  s[dfupos]=dfu_alt();
  return !dfu_exec(s.c_str());
}

bool dfu_send_bootloader(const char * fname){
  unlink(fname);
  string s=string("dfu-util -i0 -a0 -s 0x08000000 -D ")+ fname;
  s[dfupos]=dfu_alt();
  return !dfu_exec(s.c_str());
}

bool dfu_get_slot(const char * fname,int slot){
  unlink(fname);
  string s=string("dfu-util -i0 -a0 -s ");
  s[dfupos]=dfu_alt();
  switch (slot){
  case 1:
    s += "0x90000000:0x130000";
    break;
  case 2:
    s += "0x90180000:0x80000";
    break;
  default:
    return false;
  } 
  s += " -U ";
  s += fname;
  if (dfu_exec(s.c_str()))
    return false;
  // exam mode modifies flash sector at offset 0x1000 
  // restore this part to initial values
  FILE * f=fopen(fname,"rb");
  if (!f) return false;
  unsigned char buf[0x130000];
  int l=slot==1?0x130000:0x80000;
  int i=fread(buf,1,l,f);
  fclose(f);
  if (i!=l)
    return false;
  for (int j=0x1000;j<0x2000;++j)
    buf[j]=0xff;
  for (int j=0x2000;j<0x3000;++j)
    buf[j]=0;
  f=fopen(fname,"wb");
  if (!f) return false;
  i=fwrite(buf,1,l,f);
  fclose(f);
  if (i!=l)
    return false;
  return true;
}

#if 0
// check that we can really read/write on the Numworks at 0x90120000
// and get the same
// SHOULD NOT BE USED ANYMORE
bool dfu_check_epsilon2(const char * fname){
  FILE * f=fopen(fname,"wb");
  int n=0xe0000;
  char * ptr=(char *) malloc(n);
  srand(time(NULL));
  int i;
  for (i=0;i<n;++i){
    int j=(std_rand()/(1.0+RAND_MAX))*n;
    ptr[j]=std_rand();
  }
  for (i=0;i<n;++i){
    fputc(ptr[i],f);
  }
  fclose(f);
  // write to the device something that can not be guessed 
  // without really storing to flash
  string s=string("dfu-util -i0 -a0 -s 0x90120000:0xe0000:force -D ")+ fname;
  if (dfu_exec(s.c_str()))
    return false;
  // retrieve it and compare
  unlink(fname);
  s=string("dfu-util -i0 -a0 -s 0x90120000:0xe0000:force -U ")+ fname;
  s[dfupos]=dfu_alt();
  if (dfu_exec(s.c_str()))
    return false;
  f=fopen(fname,"rb");
  for (i=0;i<n;++i){
    char ch=fgetc(f);
    if (ch!=ptr[i])
      break;
  }
  fclose(f);
  return i==n;
}
#endif

// check that we can really read/write on the Numworks at 0x90740000
// and get the same
bool dfu_check_apps2(const char * fname){
  char altdfu=dfu_alt();
  FILE * f=fopen(fname,"wb");
  int n=0xa0000;
  char * ptr=(char *) malloc(n);
  srand(time(NULL));
  int i;
  for (i=0;i<n;++i){
    int j=(giac::std_rand()/(1.0+RAND_MAX))*n;
    ptr[j]=giac::std_rand();
  }
  for (i=0;i<n;++i){
    fputc(ptr[i],f);
  }
  fclose(f);
  // write to the device something that can not be guessed 
  // without really storing to flash
  string s=string("dfu-util -i0 -a0 -s 0x90740000:0xa0000 -D ")+ fname;
  if (dfu_exec(s.c_str()))
    return false;
  // retrieve it and compare
  unlink(fname);
  s=string("dfu-util -i0 -a0 -s 0x90740000:0xa0000:force -U ")+ fname;
  s[dfupos]=altdfu;
  if (dfu_exec(s.c_str()))
    return false;
  f=fopen(fname,"rb");
  for (i=0;i<n;++i){
    char ch=fgetc(f);
    if (ch!=ptr[i])
      break;
  }
  fclose(f);
  return i==n;
}

bool dfu_get_apps(const char * fname,char & altdfu){
  unlink(fname);
  string s=string("dfu-util -i0 -a0 -s 0x90200000:0x600000:force -U ")+ fname;
  s[dfupos]=altdfu;
  return !dfu_exec(s.c_str());
}

bool dfu_get_apps(const char * fname){
  char altdfu=dfu_alt();
  return dfu_get_apps(fname,altdfu);
}


char * numworks_gettar(size_t & tar_first_modif_offset){
  if (!dfu_get_apps("__apps"))
    return 0;
  FILE * f=fopen("__apps","rb");
  if (!f) return 0;
  char * buffer=(char *)malloc(numworks_maxtarsize);
  fread(buffer,numworks_maxtarsize,1,f);
  fclose(f);
  tar_first_modif_offset=tar_totalsize(buffer,numworks_maxtarsize);
  return buffer;
}

bool dfu_update_khicas(const char * fname){
  FILE * f=fopen(fname,"rb");
  if (!f) return 0;
  char * buffer=(char *)malloc(numworks_maxtarsize);
  fread(buffer,numworks_maxtarsize,1,f);
  fclose(f);
  size_t pos=0;
  char * oldbuffer=numworks_gettar(pos);
  if (!oldbuffer){
    free(buffer);
    return false;
  }
  vector<fileinfo_t> oldv=tar_fileinfo(oldbuffer,0),v=tar_fileinfo(buffer,0);
  // add files from oldbuffer that are not in buffer
  for (int i=0;i<oldv.size();++i){
    const fileinfo_t & cur=oldv[i];
    string s=cur.filename; int j;
    for (j=0;j<v.size();++j){
      if (v[j].filename==s)
	break;
    }
    if (j<v.size()) 
      continue; // file is in buffer
    int res=tar_adddata(buffer,0,s.c_str(),oldbuffer+cur.header_offset+512,cur.size,0); // exec can not be translated
  }
  file_savetar("__apps",buffer,0);
  //return true;
  return dfu_send_apps("__apps");
}

bool numworks_sendtar(char * buffer,size_t buffersize,size_t tar_first_modif_offset){
  vector<fileinfo_t> v=tar_fileinfo(buffer,buffersize);
  if (v.empty())
    return false;
  fileinfo_t info=v[v.size()-1];
  size_t end=info.header_offset+tar_filesize(info.size)+1024;
  if (end>numworks_maxtarsize || end<=tar_first_modif_offset) return false;
  for (size_t i=end-1024;i<end;++i)
    buffer[i]=0;
  FILE * f =fopen("__apps","wb");
  if (!f)
    return false;
  size_t start=(tar_first_modif_offset/65536)*65536;
  fwrite(buffer+start,end-start,1,f);
  fclose(f);
  longlong ll=0x90200000LL+start;
  string s=string("dfu-util -i 0 -a 0 -s ")+giac::gen(ll).print(giac::context0)+" -D __apps" ;
  return !dfu_exec(s.c_str());
}

#endif 

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  
  // buf:=tar "file.tar" -> init
  // tar(buf) -> list files
  // tar(buf,0,"filename") -> remove filename
  // tar(buf,1,"filename") -> add filename
  // tar(buf,2,"filename") -> save filename
  // tar(buf,"file.tar") -> write tar
  // purge(buf) -> free buffer
  gen _tar(const gen & g_,GIAC_CONTEXT){
    gen g(eval(g_,eval_level(contextptr),contextptr));
    if (g.type==_STRNG){
      char * buf=file_gettar(g._STRNGptr->c_str());
      if (!buf) return 0;
      tar_first_modified_offset=0;
      return gen((void *)buf,_BUFFER_POINTER);
    }
    if (g.type==_POINTER_ && g.subtype==_BUFFER_POINTER){
      char * buf= (char *) g._POINTER_val;
      if (!buf) return 0;
      vector<fileinfo_t> v=tar_fileinfo(buf,0);
      vecteur res1,res2,res3,res4;
      for (int i=0;i<v.size();++i){
	res1.push_back(string2gen(v[i].filename,false));
	res2.push_back(v[i].size);
	res3.push_back(v[i].header_offset);
	res4.push_back(v[i].mode);
      }
      return makevecteur(res1,res2,res3,res4);
    }
#if !defined KHICAS && !defined SDL_KHICAS && !defined USE_GMP_REPLACEMENTS && !defined GIAC_HAS_STO_38
    if (g.type==_INT_){
      if (g.val==1){
	char * buf= numworks_gettar(tar_first_modified_offset);
	if (!buf) return 0;
	return gen((void *)buf,_BUFFER_POINTER);
      }
    }
#endif
    if (g.type==_VECT){
      vecteur & v=*g._VECTptr; 
      int s=v.size();
      if (s>=2 && v.front().type==_POINTER_){
	char * buf= (char *) v[0]._POINTER_val;
	if (!buf) return 0;
	if (s==2 && v[1].type==_STRNG)
	  return file_savetar(v[1]._STRNGptr->c_str(),buf,0);
#if !defined KHICAS && !defined SDL_KHICAS && !defined USE_GMP_REPLACEMENTS && !defined GIAC_HAS_STO_38
	if (s==2 && v[1].type==_INT_){
	  if (v[1].val==1)
	    return numworks_sendtar(buf,0,tar_first_modified_offset);
	}
#endif
	if (s==3 && v[1].type==_INT_ && v[2].type==_STRNG){
	  int val=v[1].val;
	  if (val==0)
	    return tar_removefile(buf,v[2]._STRNGptr->c_str(),0);
	  if (val==1 && tar_addfile(buf,v[2]._STRNGptr->c_str(),0)){
	    gen res=gen((void *)buf,_BUFFER_POINTER);
	    if (g_.type==_VECT && !g_._VECTptr->empty())
	      return sto(res,g_._VECTptr->front(),contextptr);
	    return res;
	  }
	  if (val==2)
	    return tar_savefile(buf,v[2]._STRNGptr->c_str());
	}
      }
    }
    return gensizeerr(contextptr);
  }
  static const char _tar_s []="tar";
  static define_unary_function_eval_quoted (__tar,&_tar,_tar_s);
  define_unary_function_ptr5( at_tar ,alias_at_tar,&__tar,_QUOTE_ARGUMENTS,true);

  std::string dos2unix(const std::string & src){
    std::string unixsrc; // convert newlines to Unix
    for (int i=0;i+1<src.size();++i){
      if (src[i]==13 && src[i+1]==10)
	continue;
      unixsrc+= src[i];
    }
    return unixsrc;
  }

  gen tabunsignedchar2gen(const unsigned char tab[],int len){
    gen res=0;
    for (int i=0;i<len;++i){
      res=256*res;
      res+=tab[i];
    }
    return res;
  }
  

#if !defined KHICAS && !defined SDL_KHICAS && !defined USE_GMP_REPLACEMENTS && !defined GIAC_HAS_STO_38
  bool scriptstore2map(const char * fname,nws_map & m){
    FILE * f=fopen(fname,"rb");
    if (!f)
      return false;
    unsigned char buf[nwstoresize1];
    fread(buf,1,nwstoresize1,f);
    fclose(f);
    unsigned char * ptr=buf;
    // Numworks script store archive
    // record format: length on 2 bytes
    // if not zero length
    // record name 
    // 00
    // type 
    // record data
    if (*((unsigned *)ptr)!=0xee0bddba)  // ba dd 0b ee
      return false; 
    int pos=4; ptr+=4;
    for (;pos<nwstoresize1;){
      size_t L=ptr[1]*256+ptr[0]; 
      ptr+=2; pos+=2;
      if (L==0) break;
      L-=2;
#ifdef VISUALC
      char buf_[65536];
#else
      char buf_[L+1];
#endif
      memcpy(buf_,(const char *)ptr,L); ptr+=L; pos+=L;
      string name(buf_);
      const char * buf_mode=buf_+name.size()+2;
      nwsrec r;
      r.type=buf_mode[-1];
      r.data.resize(L-name.size()-3);
      memcpy(&r.data[0],buf_mode,r.data.size());
      m[name]=r;
    }
    if (pos>=nwstoresize1)
      return false;
    return true;
  }

  bool map2scriptstore(const nws_map & m,const char * fname){
    unsigned char buf[nwstoresize1];
    for (int i=0;i<nwstoresize1;++i)
      buf[i]=0;
    unsigned char * ptr=buf;
    *(unsigned *) ptr= 0xee0bddba;
    ptr += 4;
    nws_map::const_iterator it=m.begin(),itend=m.end();
    int total=0;
    for (;it!=itend;++it){
      const string & s=it->first;
      unsigned l1=s.size();
      unsigned l2=it->second.data.size();
      short unsigned L=2+l1+1+1+l2+1;
      total += L;
      if (total>=nwstoresize1)
	return false;
      *ptr=L % 256; ++ptr; *ptr=L/256; ++ptr;
      memcpy(ptr,s.c_str(),l1+1); ptr += l1+1;
      *ptr=it->second.type; ++ptr;
      memcpy(ptr,&it->second.data[0],l2); ptr+=l2;
      *ptr=0; ++ptr; 
    }
    FILE * f=fopen(fname,"wb");
    if (!f)
      return false;
    fwrite(buf,1,nwstoresize1,f);
    fclose(f);
    return true;
  }
#endif

  

#if !defined KHICAS && !defined SDL_KHICAS && !defined USE_GMP_REPLACEMENTS && !defined GIAC_HAS_STO_38
  const unsigned char rsa_n_tab[]=
    {
      0xf2,0x0e,0xd4,0x9d,0x44,0x04,0xc4,0xc8,0x6a,0x5b,0xc6,0x9a,0xd6,0xdf,
      0x9c,0xf5,0x56,0xf2,0x0d,0xad,0x6c,0x34,0xb4,0x48,0xf7,0xa7,0xa8,0x27,0xa0,
      0xc8,0xbe,0x36,0xb1,0xc0,0x95,0xf8,0xc2,0x72,0xfb,0x78,0x0f,0x3f,0x15,0x22,
      0xaf,0x51,0x96,0xe3,0xdc,0x39,0xb4,0xc6,0x40,0x6d,0x58,0x56,0x1f,0xad,0x55,
      0x55,0x08,0xf1,0xde,0x5a,0xbc,0xd3,0xcc,0x16,0x3d,0x33,0xee,0x83,0x3f,0x32,
      0xa7,0xa7,0xb8,0x95,0x2f,0x35,0xeb,0xf6,0x32,0x4d,0x22,0xd9,0x60,0xb7,0x5e,
      0xbd,0xea,0xa5,0xcb,0x9c,0x69,0xeb,0xfd,0x9f,0x2b,0x5f,0x3d,0x38,0x5a,0xe1,
      0x2b,0x63,0xf8,0x92,0x35,0x91,0xea,0x77,0x07,0xcc,0x4b,0x7a,0xbc,0xe0,0xa0,
      0x8b,0x82,0x98,0xa2,0x87,0x10,0x2c,0xe2,0x23,0x53,0x2f,0x70,0x03,0xec,0x2d,
      0x22,0x34,0x72,0x57,0x4d,0x24,0x2e,0x97,0xc9,0xfb,0x23,0xb0,0x05,0xff,0x87,
      0x6e,0xbf,0x94,0x2d,0xf0,0x36,0xed,0xd7,0x9a,0xac,0x0c,0x21,0x94,0xa2,0x75,
      0xfc,0x39,0x9b,0xba,0xf2,0xc6,0xc9,0x34,0xa0,0xb2,0x66,0x5a,0xcc,0xc9,0x5c,
      0xc7,0xdb,0xce,0xfb,0x3a,0x10,0xee,0xc1,0x82,0x9a,0x43,0xef,0xed,0x87,0xbd,
      0x6c,0xe4,0xc1,0x36,0xd0,0x0a,0x85,0x6e,0xca,0xcd,0x13,0x29,0x65,0xb5,0xd4,
      0x13,0x4a,0x14,0xaa,0x65,0xac,0x0e,0x6f,0x19,0xb0,0x62,0x47,0x65,0x0e,0x40,
      0x82,0x37,0xd6,0xf0,0x17,0x48,0xaa,0x8c,0x7b,0xc4,0x5e,0x4a,0x72,0x26,0xa6,
      0x08,0x2e,0xff,0x2d,0x9d,0x0e,0x2e,0x19,0xe9,0x6a,0x4c,0x7c,0x3e,0xe9,0xbc,
      0x78,0x95
    };

  int rsa_check(const char * sigfilename,int maxkeys,BYTE hash[][SHA256_BLOCK_SIZE],int * tailles,vector<string> & fnames){
    gen rsa_n(tabunsignedchar2gen(rsa_n_tab,sizeof(rsa_n_tab)));
    gen N=pow(gen(2),768),q;
    // read by blocks of 2048 bits=256 bytes
    FILE * f=fopen(sigfilename,"r");
    if (!f)
      return 0;
    char firmwarename[256];
    int i=0;
    for (;i<maxkeys;++i){
      gen key=0;
      // skip firmware filename and size
      fscanf(f,"%i %s",&tailles[i],firmwarename);
      fnames.push_back(firmwarename);
      // skip 0x prefix
      for (;;){
	unsigned char c=fgetc(f);
	if (feof(f))
	  break;
	if (c=='\n' || c==' ' || c=='0')
	  continue;
	if (c=='x')
	  break;
	// invalid char
	return 0;
      }
      if (feof(f)){
	fclose(f);
	return i;
      }
      for (int j=0;j<256;++j){
	key = 256*key;
	unsigned char c=fgetc(f);
	if (feof(f)){
	  fclose(f);
	  if (j!=0){ return 0; }
	  return i;
	}
	if (c==' ' || c=='\n')
	  break;
	if (c>='0' && c<='9')
	  c=c-'0';
	else {
	  if (c>='a' && c<='f')
	    c=10+c-'a';
	  else {
	    fclose(f);
	    return 0;
	  }
	}
	unsigned char d=fgetc(f);
	if (feof(f)){
	  fclose(f);
	  return 0;
	}
	if (d==' ' || d=='\n'){
	  key = key/16+int(c);
	  break;
	}
	if (d>='0' && d<='9')
	  d=d-'0';
	else {
	  if (d>='a' && d<='f')
	    d=10+d-'a';
	  else {
	    fclose(f);
	    return 0;
	  }
	}
	key += int(c)*16+int(d);
      }
      // public key decrypt and keep only 768 low bits
      key=powmod(key,65537,rsa_n);
      key=irem(key,N,q); 
      if (q!=12345){
	fclose(f);
	return 0;
      }
      // check that key is valid and write in hash[i]
      for (int j=0;j<32;++j){
	// divide 3 times by 256, remainder must be in '0'..'9'
	int o=0;
	int tab[]={1,10,100};
	for (int k=0;k<3;++k){
	  gen r=irem(key,256,q);
	  key=q;
	  if (r.type!=_INT_ || r.val>'9' || r.val<'0'){
	    fclose(f);
	    return 0;
	  }
	  o+=(r.val-'0')*tab[k];
	}
	if (o<0 || o>255){
	  fclose(f);
	  return 0;
	}
	if (i<maxkeys)
	  hash[i][31-j]=o;
      }
    }
    fclose(f);
    return i;
  }

  const int MAXKEYS=64;
  // Numworks firmwares signature file is in doc/shakeys
  bool sha256_check(const char * sigfilename,const char * filename,const char *firmwarename){
    BYTE hash[MAXKEYS][SHA256_BLOCK_SIZE];
    int tailles[MAXKEYS];
    vector<string> fnames;
    int nkeys=rsa_check(sigfilename,MAXKEYS,hash,tailles,fnames);
    if (nkeys==0) return false;
    BYTE buf[SHA256_BLOCK_SIZE];
    SHA256_CTX ctx;
    string text;
    FILE * f=fopen(filename,"rb");
    if (!f)
      return false;
    int taille=0;
    for (;;++taille){
      unsigned char c=fgetc(f);
      if (feof(f))
	break;
      text += c;
    }
    fclose(f);
    unsigned char * ptr=(unsigned char *)text.c_str();
    for (int i=0;i<nkeys;++i){
      if (debug_infolevel)
	CERR << i << " " << firmwarename << " " << taille << " " << fnames[i] << " " << tailles[i] << '\n';
      if (firmwarename!=fnames[i] || taille<tailles[i])
	continue;
      // now try for all compatible sizes
      // we do not know the firmware size, we extract 2M or 6M
      // the part after the end is not known
      giac_sha256_init(&ctx);
      giac_sha256_update(&ctx, ptr, tailles[i]);
      giac_sha256_final(&ctx, buf);
      if (!memcmp(hash[i], buf, SHA256_BLOCK_SIZE))
	return true;
    }
    return false;
  }

  bool nws_certify_firmware(bool withoverwrite,GIAC_CONTEXT){
    string sig(giac::giac_aide_dir()+"shakeys");
    if (!is_file_available(sig.c_str())){
#ifdef __APPLE__
      sig="/Applications/usr/share/giac/doc/shakeys";
#else
#ifdef WIN32
#ifdef __MINGW_H
      sig="c:\\xcaswin\\doc\\shakeys";
#else
      sig="/cygwin/c/xcas64/shakeys";
#endif
#else // WIN32
      sig="/usr/share/giac/doc/shakeys";
#endif // WIN32
#endif // APPLE
      if (!is_file_available(sig.c_str())){
	sig="shakeys";
	if (!is_file_available(sig.c_str())){
	  *logptr(contextptr) << "Fichier de signature introuvable\n" ;
	  return false;
	}
      }
    }
    char epsilon[]="epsilon__",apps[]="apps__";
#if 0 // internal not readable anymore
    *logptr(contextptr) << "Extraction du secteur amorce\n" ;
    if (!dfu_get_epsilon_internal(epsilon)) return false;
    *logptr(contextptr) << "Verification de signature secteur amorce\n" ;
    if (!sha256_check(sig.c_str(),epsilon,"bootloader.bin")) return false;
#endif
    *logptr(contextptr) << "Extraction du firmware externe slot 1\n" ;
    if (!dfu_get_slot(epsilon,1)) return false;
    *logptr(contextptr) << "Verification de signature externe slot 1\n" ;
    if (!sha256_check(sig.c_str(),epsilon,"khi.A.bin")) return false;
    *logptr(contextptr) << "Extraction du firmware externe slot 2\n" ;
    if (!dfu_get_slot(epsilon,2)) return false;
    *logptr(contextptr) << "Verification de signature externe slot 2\n" ;
    if (!sha256_check(sig.c_str(),epsilon,"khi.B.bin")) return false;
    *logptr(contextptr) << "Signature firmware conforme\nExtraction des applications\n" ;
    if (!dfu_get_apps(apps)) return false;
    *logptr(contextptr) << "Verification de signature applications externes\n" ;
    if (!sha256_check(sig.c_str(),apps,"apps.tar")) return false;
    const char eps2name[]="eps2__";
    if (withoverwrite && 
	( // !dfu_check_epsilon2(eps2name) || 
	  !dfu_check_apps2(eps2name)
	  )){
      *logptr(contextptr) << "Le test d'ecriture et relecture a echoue.\nLe firwmare n'est peut-etre pas conforme ou la flash est endommagee.\n";
      return false;
    }
    *logptr(contextptr) << "Signature applications conforme\nCalculatrice conforme à la reglementation\nCertification par le logiciel Xcas\nInstitut Fourier\nUniversité de Grenoble Alpes\nAssurez-vous d'avoir téléchargé Xcas sur\nwww-fourier.ujf-grenoble.fr/~parisse/install_fr.html\n" ;
    return true;
  }
#endif

  const context * python_contextptr=0;

  void opaque_double_copy(void * source,void * target){
    *((double *) target) = * ((double *) source);
  }

  double opaque_double_val(const void * source){
    longlong r = * (longlong *)(source) ;
    (* (gen *) (&r)).type = 0;
    return * (double *)(&r); 
  }

  // FIXME: make the replacement call for APPLE
  int system_no_deprecation(const char *command) {
#if defined _IOS_FIX_ || defined FXCG || defined OSXIOS
    return 0;
#else
    return system(command);
#endif
  }

  double min_proba_time=10; // in seconds

#ifdef FXCG
  void control_c(){
    int i=0 ; // KeyPressed(); // check for EXIT key pressed?
    if (i==1){ ctrl_c=true; interrupted=true; }
  }
#endif

#ifdef TIMEOUT
#if !defined(EMCC) && !defined(EMCC2)
  double time(int ){
    return double(CLOCK())/1000000; // CLOCKS_PER_SEC;
  }
#endif
  time_t caseval_begin,caseval_current;
  double caseval_maxtime=15; // max 15 seconds
  int caseval_n=0,caseval_mod=0,caseval_unitialized=-123454321;
#if !defined POCKETCAS
  void control_c(){
#if defined NSPIRE || defined KHICAS || defined SDL_KHICAS
    if (
#if defined NSPIRE || defined NSPIRE_NEWLIB
	on_key_enabled && on_key_pressed()
#else
	back_key_pressed()
#endif
	){ 
      kbd_interrupted=true;
      ctrl_c=interrupted=true; 
    }
#else
    if (caseval_unitialized!=-123454321){
      caseval_unitialized=-123454321;
      caseval_mod=0;
      caseval_n=0;
      caseval_maxtime=15;
    }
    if (caseval_mod>0){ 
      ++caseval_n; 
      if (caseval_n >=caseval_mod){
	caseval_n=0; 
	caseval_current=time(0); 
#if defined(EMCC) || defined(EMCC2)
	if (difftime(caseval_current,caseval_begin)>caseval_maxtime)
#else
	if (caseval_current>caseval_maxtime+caseval_begin)
#endif
	  { 
	    CERR << "Timeout" << '\n'; ctrl_c=true; interrupted=true; 
	    caseval_begin=caseval_current;
	  } 
      } 
    }
#endif // NSPIRE
  }
#endif // POCKETCAS
#endif // TIMEOUT

#if defined KHICAS || defined SDL_KHICAS
  void usleep(int t){
    os_wait_1ms(t/1000);
  }
#else
#ifdef NSPIRE_NEWLIB
  void usleep(int t){
    msleep(t/1000);
  }
#endif
  
#endif

#if defined VISUALC || defined BESTA_OS
#if !defined FREERTOS && !defined HAVE_LIBMPIR 
  int R_OK=4;
#endif
  int access(const char *path, int mode ){
    // return _access(path, mode );
    return 0;
  }
#if (defined RTOS_THREADX || defined VISUALC) && !defined FREERTOS && !defined WIN32
extern "C" void Sleep(unsigned int miliSecond);
#endif

#if 0
  extern "C" void Sleep(unsigned int ms){
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }
#endif

  void usleep(int t){
#ifdef RTOS_THREADX
    Sleep(t/1000);
#else
    Sleep(int(t/1000.+.5));
#endif
  }
#endif

#ifdef __APPLE__
  int PARENTHESIS_NWAIT=10;
#else
  int PARENTHESIS_NWAIT=100;
#endif

  // FIXME: threads allowed curently disabled
  // otherwise fermat_gcd_mod_2var crashes at puccini
  bool threads_allowed=true,mpzclass_allowed=true;
#ifdef HAVE_LIBPTHREAD
  pthread_mutex_t interactive_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t fork_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

  std::vector<aide> * & vector_aide_ptr (){
    static std::vector<aide> * ans = 0;
    if (!ans) ans=new std::vector<aide>;
    return ans;
  }
  std::vector<std::string> * & vector_completions_ptr (){
    static std::vector<std::string> * ans = 0;
    if (!ans) ans=new  std::vector<std::string>;
    return ans;
  }
#ifdef NSPIRE_NEWLIB
  const context * context0=new context;
#else
  const context * context0=0;
#endif
  // Global variable when context is 0
  void (*fl_widget_delete_function)(void *) =0;
#ifndef NSPIRE
  ostream & (*fl_widget_archive_function)(ostream &,void *)=0;
  gen (*fl_widget_unarchive_function)(istream &)=0;
#endif
  gen (*fl_widget_updatepict_function)(const gen & g)=0;
  std::string (*fl_widget_texprint_function)(void * ptr)=0;

  const char * _last_evaled_function_name_=0;
  const char * & last_evaled_function_name(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_last_evaled_function_name_;
    else
      return _last_evaled_function_name_;
  }

  const char * _currently_scanned=0;
  const char * & currently_scanned(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_currently_scanned_;
    else
      return _currently_scanned;
  }

  const gen * _last_evaled_argptr_=0;
  const gen * & last_evaled_argptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_last_evaled_argptr_;
    else
      return _last_evaled_argptr_;
  }

  static int _language_=0; 
  int & language(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_language_;
    else
      return _language_;
  }
  void language(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_language_=b;
#if !defined(EMCC) && !defined(EMCC2)
    else
#endif
      _language_=b;
  }

  static int _max_sum_sqrt_=3; 
  int & max_sum_sqrt(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_max_sum_sqrt_;
    else
      return _max_sum_sqrt_;
  }
  void max_sum_sqrt(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_max_sum_sqrt_=b;
    else
      _max_sum_sqrt_=b;
  }

#ifdef GIAC_HAS_STO_38 // Prime sum(x^2,x,0,100000) crash on hardware
  static int _max_sum_add_=10000; 
#else
  static int _max_sum_add_=100000; 
#endif
  int & max_sum_add(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_max_sum_add_;
    else
      return _max_sum_add_;
  }

  static int _default_color_=FL_BLACK;
  int & default_color(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_default_color_;
    else
      return _default_color_;
  }
  void default_color(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_default_color_=c;
    else
      _default_color_=c;
  }

  static void * _evaled_table_=0;
  void * & evaled_table(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_evaled_table_;
    else
      return _evaled_table_;
  }

  static void * _extra_ptr_=0;
  void * & extra_ptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_extra_ptr_;
    else
      return _extra_ptr_;
  }

  static int _spread_Row_=0;
  int & spread_Row(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_spread_Row_;
    else
      return _spread_Row_;
  }
  void spread_Row(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_spread_Row_=c;
    else
      _spread_Row_=c;
  }

  static int _spread_Col_=0;
  int & spread_Col(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_spread_Col_;
    else
      return _spread_Col_;
  }
  void spread_Col(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_spread_Col_=c;
    else
      _spread_Col_=c;
  }

  static int _printcell_current_row_=0;
  int & printcell_current_row(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_printcell_current_row_;
    else
      return _printcell_current_row_;
  }
  void printcell_current_row(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_printcell_current_row_=c;
    else
      _printcell_current_row_=c;
  }

  static int _printcell_current_col_=0;
  int & printcell_current_col(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_printcell_current_col_;
    else
      return _printcell_current_col_;
  }
  void printcell_current_col(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_printcell_current_col_=c;
    else
      _printcell_current_col_=c;
  }

  static double _total_time_=0.0;
  double & total_time(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_total_time_;
    else
      return _total_time_;
  }

#if 1
  static double _epsilon_=1e-12;
#else
#ifdef __SGI_CPP_LIMITS
  static double _epsilon_=100*numeric_limits<double>::epsilon();
#else
  static double _epsilon_=1e-12;
#endif
#endif
  double & epsilon(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_epsilon_;
    else
      return _epsilon_;
  }
  void epsilon(double c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_epsilon_=c;
    else
      _epsilon_=c;
  }

  static double _proba_epsilon_=1e-15;
  double & proba_epsilon(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_proba_epsilon_;
    else
      return _proba_epsilon_;
  }

  static bool _expand_re_im_=true; 
  bool & expand_re_im(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_expand_re_im_;
    else
      return _expand_re_im_;
  }
  void expand_re_im(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_expand_re_im_=b;
    else
      _expand_re_im_=b;
  }

  static int _scientific_format_=0; 
  int & scientific_format(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_scientific_format_;
    else
      return _scientific_format_;
  }
  void scientific_format(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_scientific_format_=b;
    else
      _scientific_format_=b;
  }

  static int _decimal_digits_=12; 

  int & decimal_digits(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_decimal_digits_;
    else
      return _decimal_digits_;
  }
  void decimal_digits(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_decimal_digits_=b;
    else
      _decimal_digits_=b;
  }

  static int _minchar_for_quote_as_string_=1; 

  int & minchar_for_quote_as_string(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_minchar_for_quote_as_string_;
    else
      return _minchar_for_quote_as_string_;
  }
  void minchar_for_quote_as_string(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_minchar_for_quote_as_string_=b;
    else
      _minchar_for_quote_as_string_=b;
  }

  static int _xcas_mode_=0; 
  int & xcas_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_xcas_mode_;
    else
      return _xcas_mode_;
  }
  void xcas_mode(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_xcas_mode_=b;
    else
      _xcas_mode_=b;
  }


  static int _integer_format_=0; 
  int & integer_format(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_integer_format_;
    else
      return _integer_format_;
  }
  void integer_format(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_integer_format_=b;
    else
      _integer_format_=b;
  }
  static int _latex_format_=0; 
  int & latex_format(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_latex_format_;
    else
      return _latex_format_;
  }
#ifdef BCD
  static u32 _bcd_decpoint_='.'|('E'<<16)|(' '<<24); 
  u32 & bcd_decpoint(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_decpoint_;
    else
      return _bcd_decpoint_;
  }

  static u32 _bcd_mantissa_=12+(15<<8); 
  u32 & bcd_mantissa(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_mantissa_;
    else
      return _bcd_mantissa_;
  }

  static u32 _bcd_flags_=0; 
  u32 & bcd_flags(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_flags_;
    else
      return _bcd_flags_;
  }

  static bool _bcd_printdouble_=false;
  bool & bcd_printdouble(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_printdouble_;
    else
      return _bcd_printdouble_;
  }

#endif

  static bool _integer_mode_=true;
  bool & integer_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_integer_mode_;
    else
      return _integer_mode_;
  }

  void integer_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_integer_mode_=b;
    else
      _integer_mode_=b;
  }

  bool python_color=false;
#ifdef NSPIRE_NEWLIB
  bool os_shell=false;
#else
  bool os_shell=true;
#endif

#ifdef KHICAS
  static int _python_compat_=true;
#else
  static int _python_compat_=false;
#endif
  int & python_compat(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_python_compat_;
    else
      return _python_compat_;
  }

  void python_compat(int b,GIAC_CONTEXT){
    python_color=b; //cout << "python_color " << b << '\n';
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_python_compat_=b;
    else
      _python_compat_=b;
  }

  static bool _complex_mode_=false; 
  bool & complex_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_complex_mode_;
    else
      return _complex_mode_;
  }

  void complex_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_complex_mode_=b;
    else
      _complex_mode_=b;
  }

  static bool _escape_real_=true; 
  bool & escape_real(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_escape_real_;
    else
      return _escape_real_;
  }

  void escape_real(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_escape_real_=b;
    else
      _escape_real_=b;
  }

  static bool _do_lnabs_=true;
  bool & do_lnabs(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_do_lnabs_;
    else
      return _do_lnabs_;
  }

  void do_lnabs(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_do_lnabs_=b;
    else
      _do_lnabs_=b;
  }

  static bool _eval_abs_=true;
  bool & eval_abs(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_eval_abs_;
    else
      return _eval_abs_;
  }

  void eval_abs(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_eval_abs_=b;
    else
      _eval_abs_=b;
  }

  static bool _eval_equaltosto_=true;
  bool & eval_equaltosto(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_eval_equaltosto_;
    else
      return _eval_equaltosto_;
  }

  void eval_equaltosto(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_eval_equaltosto_=b;
    else
      _eval_equaltosto_=b;
  }

  static bool _all_trig_sol_=false; 
  bool & all_trig_sol(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_all_trig_sol_;
    else
      return _all_trig_sol_;
  }

  void all_trig_sol(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_all_trig_sol_=b;
    else
      _all_trig_sol_=b;
  }

  static bool _try_parse_i_=true; 
  bool & try_parse_i(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_try_parse_i_;
    else
      return _try_parse_i_;
  }

  void try_parse_i(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_try_parse_i_=b;
    else
      _try_parse_i_=b;
  }

  static bool _specialtexprint_double_=false; 
  bool & specialtexprint_double(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_specialtexprint_double_;
    else
      return _specialtexprint_double_;
  }

  void specialtexprint_double(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_specialtexprint_double_=b;
    else
      _specialtexprint_double_=b;
  }

  static bool _atan_tan_no_floor_=false; 
  bool & atan_tan_no_floor(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_atan_tan_no_floor_;
    else
      return _atan_tan_no_floor_;
  }

  void atan_tan_no_floor(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_atan_tan_no_floor_=b;
    else
      _atan_tan_no_floor_=b;
  }

  static bool _keep_acosh_asinh_=false; 
  bool & keep_acosh_asinh(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_keep_acosh_asinh_;
    else
      return _keep_acosh_asinh_;
  }

  void keep_acosh_asinh(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_keep_acosh_asinh_=b;
    else
      _keep_acosh_asinh_=b;
  }

  static bool _keep_algext_=false; 
  bool & keep_algext(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_keep_algext_;
    else
      return _keep_algext_;
  }

  void keep_algext(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_keep_algext_=b;
    else
      _keep_algext_=b;
  }

  static bool _auto_assume_=false; 
  bool & auto_assume(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_auto_assume_;
    else
      return _auto_assume_;
  }

  void auto_assume(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_auto_assume_=b;
    else
      _auto_assume_=b;
  }

  static bool _parse_e_=false; 
  bool & parse_e(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_parse_e_;
    else
      return _parse_e_;
  }

  void parse_e(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_parse_e_=b;
    else
      _parse_e_=b;
  }

  static bool _convert_rootof_=true; 
  bool & convert_rootof(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_convert_rootof_;
    else
      return _convert_rootof_;
  }

  void convert_rootof(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_convert_rootof_=b;
    else
      _convert_rootof_=b;
  }

  static bool _lexer_close_parenthesis_=true; 
  bool & lexer_close_parenthesis(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_lexer_close_parenthesis_;
    else
      return _lexer_close_parenthesis_;
  }

  void lexer_close_parenthesis(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_lexer_close_parenthesis_=b;
    else
      _lexer_close_parenthesis_=b;
  }

  static bool _rpn_mode_=false; 
  bool & rpn_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_rpn_mode_;
    else
      return _rpn_mode_;
  }

  void rpn_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_rpn_mode_=b;
    else
      _rpn_mode_=b;
  }

#ifdef __MINGW_H
  static bool _ntl_on_=false; 
#else
  static bool _ntl_on_=true; 
#endif

  bool & ntl_on(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_ntl_on_;
    else
      return _ntl_on_;
  }

  void ntl_on(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_ntl_on_=b;
    else
      _ntl_on_=b;
  }

  static bool _complex_variables_=false; 
  bool & complex_variables(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_complex_variables_;
    else
      return _complex_variables_;
  }

  void complex_variables(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_complex_variables_=b;
    else
      _complex_variables_=b;
  }

  static bool _increasing_power_=false;
  bool & increasing_power(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_increasing_power_;
    else
      return _increasing_power_;
  }

  void increasing_power(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_increasing_power_=b;
    else
      _increasing_power_=b;
  }

  static vecteur & _history_in_(){
    static vecteur * ans = 0;
    if (ans)
      ans=new vecteur;
    return *ans;
  }
  vecteur & history_in(GIAC_CONTEXT){
    if (contextptr)
      return *contextptr->history_in_ptr;
    else
      return _history_in_();
  }

  static vecteur & _history_out_(){
    static vecteur * ans = 0;
    if (!ans)
      ans=new vecteur;
    return *ans;
  }
  vecteur & history_out(GIAC_CONTEXT){
    if (contextptr)
      return *contextptr->history_out_ptr;
    else
      return _history_out_();
  }

  static vecteur & _history_plot_(){
    static vecteur * ans = 0;
    if (!ans)
      ans=new vecteur;
    return *ans;
  }
  vecteur & history_plot(GIAC_CONTEXT){
    if (contextptr){
      vecteur * hist=contextptr->history_plot_ptr;
      if (hist->size()>=256)
        hist->erase(hist->begin(),hist->end()-128);
      return *hist;
    }
    else
      return _history_plot_();
  }

  static bool _approx_mode_=false;
  bool & approx_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_approx_mode_;
    else
      return _approx_mode_;
  }

  void approx_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_approx_mode_=b;
    else
      _approx_mode_=b;
  }

  static char _series_variable_name_='h';
  char & series_variable_name(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_series_variable_name_;
    else
      return _series_variable_name_;
  }

  void series_variable_name(char b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_series_variable_name_=b;
    else
      _series_variable_name_=b;
  }

  static unsigned short _series_default_order_=5;
  unsigned short & series_default_order(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_series_default_order_;
    else
      return _series_default_order_;
  }

  void series_default_order(unsigned short b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_series_default_order_=b;
    else
      _series_default_order_=b;
  }

  static int _angle_mode_=0;
  bool angle_radian(GIAC_CONTEXT)
  {
    if(contextptr && contextptr->globalptr)
      return contextptr->globalptr->_angle_mode_ == 0;
    else
      return _angle_mode_ == 0;
  }

  void angle_radian(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_angle_mode_=(b?0:1);
    else
      _angle_mode_=(b?0:1);
  }

  bool angle_degree(GIAC_CONTEXT)
  {
    if(contextptr && contextptr->globalptr)
      return contextptr->globalptr->_angle_mode_ == 1;
    else
      return _angle_mode_ == 1;
  }

  int get_mode_set_radian(GIAC_CONTEXT)
  {
    int mode;
    if(contextptr && contextptr->globalptr)
    {
      mode = contextptr->globalptr->_angle_mode_;
      contextptr->globalptr->_angle_mode_ = 0;
    }
    else
    {
      mode = _angle_mode_;
      _angle_mode_ = 0;
    }
    return mode;
  }

  void angle_mode(int b, GIAC_CONTEXT)
  {
    if(contextptr && contextptr->globalptr){
#ifdef POCKETCAS
      _angle_mode_ = b;
#endif
      contextptr->globalptr->_angle_mode_ = b;
    }
    else
      _angle_mode_ = b;
  }

  int & angle_mode(GIAC_CONTEXT)
  {
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_angle_mode_;
    else
      return _angle_mode_;
  }

  static bool _show_point_=true;
  bool & show_point(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_show_point_;
    else
      return _show_point_;
  }

  void show_point(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_show_point_=b;
    else
      _show_point_=b;
  }

  static int _show_axes_=1;
  int & show_axes(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_show_axes_;
    else
      return _show_axes_;
  }

  void show_axes(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_show_axes_=b;
    else
      _show_axes_=b;
  }

  static bool _io_graph_=false; 
  // DO NOT SET TO true WITH non-zero contexts or fix symadd when points are added
  bool & io_graph(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_io_graph_;
    else
      return _io_graph_;
  }

  void io_graph(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_io_graph_=b;
    else
      _io_graph_=b;
  }

  static bool _variables_are_files_=false;
  bool & variables_are_files(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_variables_are_files_;
    else
      return _variables_are_files_;
  }

  void variables_are_files(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_variables_are_files_=b;
    else
      _variables_are_files_=b;
  }

  static int _bounded_function_no_=0;
  int & bounded_function_no(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bounded_function_no_;
    else
      return _bounded_function_no_;
  }

  void bounded_function_no(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_bounded_function_no_=b;
    else
      _bounded_function_no_=b;
  }

  static int _series_flags_=0x3;
  int & series_flags(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_series_flags_;
    else
      return _series_flags_;
  }

  void series_flags(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_series_flags_=b;
    else
      _series_flags_=b;
  }

  static int _step_infolevel_=0;
  int & step_infolevel(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_step_infolevel_;
    else
      return _step_infolevel_;
  }

  void step_infolevel(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_step_infolevel_=b;
    else
      _step_infolevel_=b;
  }

  static bool _local_eval_=true;
  bool & local_eval(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_local_eval_;
    else
      return _local_eval_;
  }

  void local_eval(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_local_eval_=b;
    else
      _local_eval_=b;
  }

  static bool _withsqrt_=true;
  bool & withsqrt(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_withsqrt_;
    else
      return _withsqrt_;
  }

  void withsqrt(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_withsqrt_=b;
    else
      _withsqrt_=b;
  }

#ifdef WITH_MYOSTREAM
  my_ostream my_cerr (&CERR);
  static my_ostream * _logptr_= &my_cerr;

  my_ostream * logptr(GIAC_CONTEXT){
    my_ostream * res;
    if (contextptr && contextptr->globalptr )
      res=contextptr->globalptr->_logptr_;
    else
      res= _logptr_;
    return res?res:&my_cerr;
  }
#else
#ifdef NSPIRE
  static nio::console * _logptr_=&CERR;
  nio::console * logptr(GIAC_CONTEXT){
    return &CERR;
  }
#else
#ifdef FXCG
  static ostream * _logptr_=0;
#else
#if defined KHICAS || defined SDL_KHICAS
  stdostream os_cerr;
  static my_ostream * _logptr_=&os_cerr;
#else
  static my_ostream * _logptr_=&CERR;
#endif
#endif
  my_ostream * logptr(GIAC_CONTEXT){
    my_ostream * res;
    if (contextptr && contextptr->globalptr )
      res=contextptr->globalptr->_logptr_;
    else
      res= _logptr_;
#if (defined(EMCC) || defined(EMCC2)) && !defined SDL_KHICAS
    return res?res:&COUT;
#else
#ifdef FXCG
    return 0;
#else
#if defined KHICAS || defined SDL_KHICAS
    return res?res:&os_cerr;
#else
    return res?res:&CERR;
#endif
#endif
#endif
  }
#endif
#endif

  void logptr(my_ostream * b,GIAC_CONTEXT){
#ifdef NSPIRE
#else
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_logptr_=b;
    else
      _logptr_=b;
#endif
  }

  thread_param::thread_param(): _kill_thread(0), thread_eval_status(-1), v(6)
#ifdef HAVE_LIBPTHREAD
#ifdef __MINGW_H
			      ,eval_thread(),stackaddr(0)
#else
			      ,eval_thread(0),stackaddr(0)
#endif
#endif
    ,stack(0)
  { 
  }

  thread_param * & context0_thread_param_ptr(){
    static thread_param * ans=0;
    if (!ans)
      ans=new thread_param();
    return ans;
  }

#if 0
  static thread_param & context0_thread_param(){
    return *context0_thread_param_ptr();
  }
#endif

  thread_param * thread_param_ptr(const context * contextptr){
    return (contextptr && contextptr->globalptr)?contextptr->globalptr->_thread_param_ptr:context0_thread_param_ptr();
  }

  int kill_thread(GIAC_CONTEXT){
    thread_param * ptr= (contextptr && contextptr->globalptr )?contextptr->globalptr->_thread_param_ptr:0;
    return ptr?ptr->_kill_thread:context0_thread_param_ptr()->_kill_thread;
  }

  void kill_thread(int b,GIAC_CONTEXT){
    thread_param * ptr= (contextptr && contextptr->globalptr )?contextptr->globalptr->_thread_param_ptr:0;
    if (!ptr)
      ptr=context0_thread_param_ptr();
    ptr->_kill_thread=b;
  }


#ifdef HAVE_LIBPTHREAD
  pthread_mutex_t _mutexptr = PTHREAD_MUTEX_INITIALIZER,_mutex_eval_status= PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t * mutexptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      return contextptr->globalptr->_mutexptr;
    return &_mutexptr;
  }

  bool is_context_busy(GIAC_CONTEXT){
    int concurrent=pthread_mutex_trylock(mutexptr(contextptr));
    bool res=concurrent==EBUSY;
    if (!res)
      pthread_mutex_unlock(mutexptr(contextptr));
    return res;
  }

  int thread_eval_status(GIAC_CONTEXT){
    int res;
    if (contextptr && contextptr->globalptr){
      pthread_mutex_lock(contextptr->globalptr->_mutex_eval_status_ptr);
      res=contextptr->globalptr->_thread_param_ptr->thread_eval_status;
      pthread_mutex_unlock(contextptr->globalptr->_mutex_eval_status_ptr);
    }
    else {
      pthread_mutex_lock(&_mutex_eval_status);
      res=context0_thread_param_ptr()->thread_eval_status;
      pthread_mutex_unlock(&_mutex_eval_status);
    }
    return res;
  }

  void thread_eval_status(int val,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr){
      pthread_mutex_lock(contextptr->globalptr->_mutex_eval_status_ptr);
      contextptr->globalptr->_thread_param_ptr->thread_eval_status=val;
      pthread_mutex_unlock(contextptr->globalptr->_mutex_eval_status_ptr);
    }
    else {
      pthread_mutex_lock(&_mutex_eval_status);
      context0_thread_param_ptr()->thread_eval_status=val;
      pthread_mutex_unlock(&_mutex_eval_status);
    }
  }

#else
  bool is_context_busy(GIAC_CONTEXT){
    return false;
  }

  int thread_eval_status(GIAC_CONTEXT){
    return -1;
  }
  
  void thread_eval_status(int val,GIAC_CONTEXT){
  }

#endif

  static int _eval_level=DEFAULT_EVAL_LEVEL;
  int & eval_level(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_eval_level;
    else
      return _eval_level;
  }

  void eval_level(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_eval_level=b;
    else
      _eval_level=b;
  }

#ifdef FXCG // defined(GIAC_HAS_STO_38) || defined(ConnectivityKit)
  static unsigned int _rand_seed=123457;
#else
  static tinymt32_t _rand_seed;
#endif

#ifdef FXCG // defined(GIAC_HAS_STO_38) || defined(ConnectivityKit)
  unsigned int & rand_seed(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_rand_seed;
    else
      return _rand_seed;
  }
#else
  tinymt32_t * rand_seed(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return &contextptr->globalptr->_rand_seed;
    else
      return &_rand_seed;
  }
#endif

  void rand_seed(unsigned int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_rand_seed=b;
    else
      _rand_seed=b;
  }

  int std_rand(){
#if 1 // def NSPIRE
    static unsigned int r = 0;
    r = unsigned ((1664525*ulonglong(r)+1013904223)%(ulonglong(1)<<31));
    return r;
#else
    return std::rand();
#endif
  }

  int giac_rand(GIAC_CONTEXT){
#ifdef FXCG // defined(GIAC_HAS_STO_38) || defined(ConnectivityKit)
    unsigned int & r = rand_seed(contextptr);
    // r = (2147483629*ulonglong(r)+ 2147483587)% 2147483647;
    r = unsigned ((1664525*ulonglong(r)+1013904223)%(ulonglong(1)<<31));
    return r;
#else
    for (;;){
      unsigned r=tinymt32_generate_uint32(rand_seed(contextptr)) >> 1;
      if (!(r>>31))
	return r;
    }
#endif // tinymt32
  }

  static int _prog_eval_level_val=1;
  int & prog_eval_level_val(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_prog_eval_level_val;
    else
      return _prog_eval_level_val;
  }

  void prog_eval_level_val(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_prog_eval_level_val=b;
    else
      _prog_eval_level_val=b;
  }

  void cleanup_context(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr ){
      contextptr->globalptr->_eval_level=DEFAULT_EVAL_LEVEL;
    }
    eval_level(contextptr)=DEFAULT_EVAL_LEVEL;
    if (!contextptr)
      protection_level=0;
    local_eval(true,contextptr);
  }


  static parser_lexer & _pl(){
    static parser_lexer * ans = 0;
    if (!ans)
      ans=new parser_lexer();
    ans->_i_sqrt_minus1_=1;
    return * ans;
  }
  int & lexer_column_number(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._lexer_column_number_;
    else
      return _pl()._lexer_column_number_;
  }
  int & lexer_line_number(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._lexer_line_number_;
    else
      return _pl()._lexer_line_number_;
  }
  void lexer_line_number(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._lexer_line_number_=b;
    else
      _pl()._lexer_line_number_=b;
  }
  void increment_lexer_line_number(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      ++contextptr->globalptr->_pl._lexer_line_number_;
    else
      ++_pl()._lexer_line_number_;
  }

  int & index_status(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._index_status_;
    else
      return _pl()._index_status_;
  }
  void index_status(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._index_status_=b;
    else
      _pl()._index_status_=b;
  }

  int & i_sqrt_minus1(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._i_sqrt_minus1_;
    else
      return _pl()._i_sqrt_minus1_;
  }
  void i_sqrt_minus1(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._i_sqrt_minus1_=b;
    else
      _pl()._i_sqrt_minus1_=b;
  }

  int & opened_quote(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._opened_quote_;
    else
      return _pl()._opened_quote_;
  }
  void opened_quote(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._opened_quote_=b;
    else
      _pl()._opened_quote_=b;
  }

  int & in_rpn(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._in_rpn_;
    else
      return _pl()._in_rpn_;
  }
  void in_rpn(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._in_rpn_=b;
    else
      _pl()._in_rpn_=b;
  }

  int & spread_formula(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._spread_formula_;
    else
      return _pl()._spread_formula_;
  }
  void spread_formula(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._spread_formula_=b;
    else
      _pl()._spread_formula_=b;
  }

  int & initialisation_done(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._initialisation_done_;
    else
      return _pl()._initialisation_done_;
  }
  void initialisation_done(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._initialisation_done_=b;
    else
      _pl()._initialisation_done_=b;
  }

  static int _calc_mode_=0; 
  int & calc_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_calc_mode_;
    else
      return _calc_mode_;
  }
  int abs_calc_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return absint(contextptr->globalptr->_calc_mode_);
    else
      return absint(_calc_mode_);
  }
  static std::string & _autoname_(){
    static string * ans = 0;
    if (!ans){
#ifdef GIAC_HAS_STO_38
      ans= new string("GA");
#else
      ans = new string("A");
#endif
    }
    return *ans;
  }
  void calc_mode(int b,GIAC_CONTEXT){
    if ( (b==38 || b==-38) && strcmp(_autoname_().c_str(),"GA")<0)
      autoname("GA",contextptr);
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_calc_mode_=b;
    else
      _calc_mode_=b;
  }

  int array_start(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr){
      bool hp38=absint(contextptr->globalptr->_calc_mode_)==38;
      return (!contextptr->globalptr->_python_compat_ && (contextptr->globalptr->_xcas_mode_ || hp38))?1:0;
    }
    return (!_python_compat_ && (_xcas_mode_ || absint(_calc_mode_)==38))?1:0;
  }

  std::string autoname(GIAC_CONTEXT){
    std::string res;
    if (contextptr && contextptr->globalptr )
      res=contextptr->globalptr->_autoname_;
    else
      res=_autoname_();
    for (;;){
      gen tmp(res,contextptr);
      if (tmp.type==_IDNT){
	gen tmp1=eval(tmp,1,contextptr);
	if (tmp==tmp1)
	  break;
      }
      autoname_plus_plus(res);
    }
    return res;
  }
  std::string autoname(const std::string & s,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_autoname_=s;
    else
      _autoname_()=s;
    return s;
  }

  static std::string & _autosimplify_(){
    static string * ans = 0;
    if (!ans)
      ans=new string("regroup");
    return *ans;
  }
  std::string autosimplify(GIAC_CONTEXT){
    std::string res;
    if (contextptr && contextptr->globalptr )
      res=contextptr->globalptr->_autosimplify_;
    else
      res=_autosimplify_();
    return res;
  }
  std::string autosimplify(const std::string & s,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_autosimplify_=s;
    else
      _autosimplify_()=s;
    return s;
  }

  static std::string & _lastprog_name_(){
    static string * ans = 0;
    if (!ans)
      ans=new string("lastprog");
    return *ans;
  }
  std::string lastprog_name(GIAC_CONTEXT){
    std::string res;
    if (contextptr && contextptr->globalptr )
      res=contextptr->globalptr->_lastprog_name_;
    else
      res=_lastprog_name_();
    return res;
  }
  std::string lastprog_name(const std::string & s,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_lastprog_name_=s;
    else
      _lastprog_name_()=s;
    return s;
  }

  static std::string & _format_double_(){
    static string * ans = 0;
    if (!ans)
      ans=new string("");
    return * ans;
  }
  std::string & format_double(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_format_double_;
    else
      return _format_double_();
  }

  std::string comment_s(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._comment_s_;
    else
      return _pl()._comment_s_;
  }
  void comment_s(const std::string & b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._comment_s_=b;
    else
      _pl()._comment_s_=b;
  }

  void increment_comment_s(const std::string & b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._comment_s_ += b;
    else
      _pl()._comment_s_ += b;
  }

  void increment_comment_s(char b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._comment_s_ += b;
    else
      _pl()._comment_s_ += b;
  }

  std::string parser_filename(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._parser_filename_;
    else
      return _pl()._parser_filename_;
  }
  void parser_filename(const std::string & b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._parser_filename_=b;
    else
      _pl()._parser_filename_=b;
  }

  std::string parser_error(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._parser_error_;
    else
      return _pl()._parser_error_;
  }
  void parser_error(const std::string & b,GIAC_CONTEXT){
#ifndef GIAC_HAS_STO_38
    if (!first_error_line(contextptr))
      alert(b,contextptr);
    else
      *logptr(contextptr) << b << '\n';
#endif
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._parser_error_=b;
    else
      _pl()._parser_error_=b;
  }

  std::string error_token_name(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._error_token_name_;
    else
      return _pl()._error_token_name_;
  }
  void error_token_name(const std::string & b0,GIAC_CONTEXT){
    string b(b0);
    if (b0.size()==2 && b0[0]==-61 && b0[1]==-65)
      b="end of input";
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._error_token_name_=b;
    else
      _pl()._error_token_name_=b;
  }

  int & first_error_line(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._first_error_line_;
    else
      return _pl()._first_error_line_;
  }
  void first_error_line(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._first_error_line_=b;
    else
      _pl()._first_error_line_=b;
  }

  static gen & _parsed_gen_(){
    static gen * ans = 0;
    if (!ans)
      ans=new gen;
    return * ans;
  }
  gen parsed_gen(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return *contextptr->globalptr->_parsed_genptr_;
    else
      return _parsed_gen_();
  }
  void parsed_gen(const gen & b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      *contextptr->globalptr->_parsed_genptr_=b;
    else
      _parsed_gen_()=b;
  }

  static logo_turtle & _turtle_(){
    static logo_turtle * ans = 0;
    if (!ans)
      ans=new logo_turtle;
    return *ans;
  }
  logo_turtle & turtle(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_turtle_;
    else
      return _turtle_();
  }

#if !defined KHICAS && !defined SDL_KHICAS
  // protect turtle access by a lock
  // turtle changes are mutually exclusive even in different contexts
#ifdef HAVE_LIBPTHREAD
  pthread_mutex_t turtle_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
  std::vector<logo_turtle> & _turtle_stack_(){
    static std::vector<logo_turtle> * ans = 0;
    if (!ans)
      ans=new std::vector<logo_turtle>(1,_turtle_());
#ifdef HAVE_LIBPTHREAD
    ans->reserve(20000);
#endif
    return *ans;
  }
  std::vector<logo_turtle> & turtle_stack(GIAC_CONTEXT){
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&turtle_mutex);
#endif
    std::vector<logo_turtle> * ans=0;
    if (contextptr && contextptr->globalptr )
      ans=&contextptr->globalptr->_turtle_stack_;
    else
      ans=&_turtle_stack_();
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_unlock(&turtle_mutex);
#endif
    return *ans;
  }
#endif

  // Other global variables
#ifdef NSPIRE
  bool secure_run=false;
#else
  bool secure_run=true;
#endif
  bool center_history=false;
  bool in_texmacs=false;
  bool block_signal=false;
  bool CAN_USE_LAPACK = true;
  bool simplify_sincosexp_pi=true;
  int history_begin_level=0; 
  // variable used to avoid copying the whole history between processes 
#ifdef WIN32 // Temporary
  int debug_infolevel=0;
#else
  int debug_infolevel=0;
#endif
  int printprog=0;
#if defined __APPLE__ || defined VISUALC || defined __MINGW_H || defined BESTA_OS || defined NSPIRE || defined FXCG || defined NSPIRE_NEWLIB || defined KHICAS || defined SDL_KHICAS
#ifdef _WIN32
  int threads=atoi(getenv("NUMBER_OF_PROCESSORS"));
#else
  int threads=1;
#endif
#else
  int threads=sysconf (_SC_NPROCESSORS_ONLN);
#endif
  unsigned max_pairs_by_iteration=32768; 
  // gbasis max number of pairs by F4 iteration
  // setting to 2000 accelerates cyclic9mod but cyclic9 would be slower
  // 32768 is enough for cyclic10mod without truncation and not too large for yang1
  unsigned simult_primes=20,simult_primes2=20,simult_primes3=20,simult_primes_seuil2=-1,simult_primes_seuil3=-1; 
  // gbasis modular algorithm on Q: simultaneous primes (more primes means more parallel threads but also more memory required)
  double gbasis_reinject_ratio=0.2;
  // gbasis modular algo on Q: if new basis element exceed this ratio, new elements are reinjected in the ideal generators for the remaining computations
  double gbasis_reinject_speed_ratio=1./8; // modified from 1/6. for cyclic8
  // gbasis modular algo on Q: new basis elements are reinjected if the 2nd run with learning CPU speed / 1st run without learning CPU speed is >=
  int gbasis_logz_age_sort=0,gbasis_stop=0;
  // rur_do_gbasis==-1 no gbasis Q recon for rur, ==0 always gbasis Q recon, >0 size limit in monomials of the gbasis for gbasis Q recon
  // rur_do_certify==-1 do not certify, ==0 full certify, >0 certify equation if total degree is <= rur_do_certify. Beware of the 1 shift with the user command.
  int rur_do_gbasis=-1,rur_do_certify=0,rur_certify_maxthreads=6;
  bool rur_error_ifnot0dimensional=false;
  unsigned short int GIAC_PADIC=50;
  const char cas_suffixe[]=".cas";
  int MAX_PROD_EXPAND_SIZE=4096;
  int MAX_SIMPLIFIER_VECTSIZE=256;
  int ABERTH_NMAX=25;
  int ABERTH_NBITSMAX=8192;
  int LAZY_ALG_EXT=0;
  int ALG_EXT_DIGITS=180;
#if defined RTOS_THREADX || defined BESTA_OS || defined(KHICAS) || defined SDL_KHICAS
#ifdef BESTA_OS
  int LIST_SIZE_LIMIT = 100000 ;
  int FACTORIAL_SIZE_LIMIT = 1000 ;
  int CALL_LAPACK = 1111;
#else
  int LIST_SIZE_LIMIT = 1000 ;
  int FACTORIAL_SIZE_LIMIT = 254 ;
  int CALL_LAPACK = 1111;
#endif
  int GAMMA_LIMIT = 100 ;
  int NEWTON_DEFAULT_ITERATION=40;
  int NEWTON_MAX_RANDOM_RESTART=5;
  int TEST_PROBAB_PRIME=25;
  int GCDHEU_MAXTRY=5;
  int GCDHEU_DEGREE=100;
  int DEFAULT_EVAL_LEVEL=5;
  int MODFACTOR_PRIMES =5;
  int NTL_MODGCD=1<<30; // default: ntl gcd disabled
  int NTL_RESULTANT=382; 
  int NTL_XGCD=50;
  int HGCD=128;//16384;
  int HENSEL_QUADRATIC_POWER=25;
  int KARAMUL_SIZE=13;
  int INT_KARAMUL_SIZE=300;
  int FFTMUL_SIZE=100; 
  int FFTMUL_INT_MAXBITS=1024;
  int MAX_ALG_EXT_ORDER_SIZE = 4;
  int MAX_COMMON_ALG_EXT_ORDER_SIZE = 16;
  int TRY_FU_UPRIME=5;
  int TRY_FU_UPRIME_MAXLEAFSIZE=128;
  int SOLVER_MAX_ITERATE=25;
  int MAX_PRINTABLE_ZINT=10000;
  int MAX_RECURSION_LEVEL=9;
  int GBASIS_COEFF_STRATEGY=0;
  float GBASIS_COEFF_MAXLOGRATIO=2;
  int GBASIS_DETERMINISTIC=20;
  int GBASISF4_MAX_TOTALDEG=1024;
  int GBASISF4_MAXITER=256;
  int RUR_PARAM_MAX_DEG=128;
  // int GBASISF4_BUCHBERGER=5;
  const int BUFFER_SIZE=512;
#else
  int CALL_LAPACK=1111;
#if defined(EMCC) || defined(EMCC2)
  int LIST_SIZE_LIMIT = 10000000 ;
#else
  int LIST_SIZE_LIMIT = 500000000 ;
#endif
#ifdef USE_GMP_REPLACEMENTS
  int FACTORIAL_SIZE_LIMIT = 10000 ;
#else
  int FACTORIAL_SIZE_LIMIT = 10000000 ;
#endif
  int GAMMA_LIMIT = 100 ;
  int NEWTON_DEFAULT_ITERATION=60;
#ifdef GIAC_GGB
  int NEWTON_MAX_RANDOM_RESTART=20;
#else
  int NEWTON_MAX_RANDOM_RESTART=5;
#endif
  int TEST_PROBAB_PRIME=25;
  int GCDHEU_MAXTRY=5;
  int GCDHEU_DEGREE=100;
  int DEFAULT_EVAL_LEVEL=25;
  int MODFACTOR_PRIMES =5;
  int NTL_MODGCD=1<<30; // default: ntl gcd disabled
  int NTL_RESULTANT=382; 
  int NTL_XGCD=50;
  int HGCD=128;//16384;
  int HENSEL_QUADRATIC_POWER=25;
  int KARAMUL_SIZE=13;
  int INT_KARAMUL_SIZE=300;
  int FFTMUL_SIZE=100; 
  int FFTMUL_INT_MAXBITS=1024;
#if 0 // def GIAC_GGB
  int MAX_ALG_EXT_ORDER_SIZE = 3;
#else
  int MAX_ALG_EXT_ORDER_SIZE = 6;
#endif
#if defined EMCC || defined NO_TEMPLATE_MULTGCD || defined GIAC_HAS_STO_38
  int MAX_COMMON_ALG_EXT_ORDER_SIZE = 16;
#else
  int MAX_COMMON_ALG_EXT_ORDER_SIZE = 64;
#endif
  int TRY_FU_UPRIME=5;
  int TRY_FU_UPRIME_MAXLEAFSIZE=128;
  int SOLVER_MAX_ITERATE=25;
  int MAX_PRINTABLE_ZINT=1000000;
  int MAX_RECURSION_LEVEL=100;
  int GBASIS_COEFF_STRATEGY=0;
  float GBASIS_COEFF_MAXLOGRATIO=2;
  int GBASIS_DETERMINISTIC=50;
  int GBASISF4_MAX_TOTALDEG=16384;
  int GBASISF4_MAXITER=1024;
  int RUR_PARAM_MAX_DEG=128;
  // int GBASISF4_BUCHBERGER=5;
  const int BUFFER_SIZE=16384;
#endif
  volatile bool ctrl_c=false,interrupted=false,kbd_interrupted=false;
#ifdef GIAC_HAS_STO_38
  double powlog2float=1e4*10; // increase max int size for HP Prime
  int MPZ_MAXLOG2=8600*10; // max 2^8600 about 1K*10
#else
  double powlog2float=1e8;
  int MPZ_MAXLOG2=80000000; // 100 millions bits
#endif
#ifdef HAVE_LIBNTL
  int PROOT_FACTOR_MAXDEG=300;
#else
  int PROOT_FACTOR_MAXDEG=30;
#endif
  int MODRESULTANT=20;
  int ABS_NBITS_EVALF=1000;
  int SET_COMPARE_MAXIDNT=20;

  // used by WIN32 for the path to the xcas directory
  string & xcasroot(){
    static string * ans=0;
    if (!ans)
      ans=new string;
    return * ans;
  }
  string & xcasrc(){
#ifdef WIN32
    static string * ans=0;
    if (!ans) ans=new string("xcas.rc");
#else
    static string * ans=0;
    if (!ans) ans=new string(".xcasrc");
#endif
    return *ans;
  }

#if defined HAVE_SIGNAL_H && !defined HAVE_NO_SIGNAL_H
  pid_t parent_id=getpid();
#else
  pid_t parent_id=0;
#endif
  pid_t child_id=0; // child process (to replace by a vector of childs?)

  void ctrl_c_signal_handler(int signum){
    ctrl_c=true;
#if !defined KHICAS && !defined SDL_KHICAS && !defined NSPIRE_NEWLIB && !defined WIN32 && !defined BESTA_OS && !defined NSPIRE && !defined FXCG && !defined POCKETCAS && !defined __MINGW_H
    if (child_id)
      kill(child_id,SIGINT);
#endif
#if defined HAVE_SIGNAL_H && !defined HAVE_NO_SIGNAL_H
    cerr << "Ctrl-C pressed (pid " << getpid() << ")" << '\n';
#endif
  }
#if !defined NSPIRE && !defined FXCG
  gen catch_err(const std::runtime_error & error){
    cerr << error.what() << '\n';
    debug_ptr(0)->sst_at_stack.clear();
    debug_ptr(0)->current_instruction_stack.clear();
    debug_ptr(0)->args_stack.clear();
    protection_level=0;
    debug_ptr(0)->debug_mode=false;
    return string2gen(string(error.what()),false);
  }
#endif

#if 0
  static vecteur subvect(const vecteur & v,int i){
    int s=v.size();
    if (i<0)
      i=-i;
    vecteur res(v);
    for (;s<i;++s)
      res.push_back(undef);
    return vecteur(res.begin(),res.begin()+i);
  }
#endif

#if defined HAVE_SIGNAL_H_OLD || defined HAVE_SIGNAL_H
  //#define SIGNALDBG
  int forkmaxleafsize=2048; // change by fork_timeout(n)
  static bool running_file=false;
  static int run_modif_pos;
  bool synchronize_history=true;
  char buf[BUFFER_SIZE];

  // at the beginning the parent process calls make_child
  // this forks, the child process then waits for a SIGUSR1 from
  // the parent that indicate data to evaluate is ready in #cas_entree#
  // When finished the child kills the parent with SIGUSR1
  // The answer is in #cas_sortie#
  // Parent: child_busy is set to true during evaluation
  // and reset to false by SIGUSR1
  // Child uses signal_child for signal trapping and parent uses
  // data_ready
  // SIGUSR2 is used by the child process for intermediate data
  // [Typically a plot instruction]
  // The child kills the parent with SIGUSR2,
  // child_busy remains true but data ready is set
  // Then the child waits for a parent SIGUSR2 signal
  // signal_plot_parent is true for intermediate data, false otherwise
  volatile bool signal_child=true; // true if busy
  volatile bool signal_plot_child=false; // true if child can continue
  volatile bool signal_plot_parent=false; 
  volatile bool child_busy=false,data_ready=false;
  // child sends a SIGUSR1
  void data_signal_handler(int signum){
#ifdef SIGNALDBG
    cerr << "Parent: child signaled data ready" << '\n';
#endif
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&fork_mutex);
#endif
    signal(SIGUSR1,SIG_IGN);
    signal_plot_parent=false;
    child_busy=false;
    data_ready=true;
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_unlock(&fork_mutex);
#endif
  }

  void child_launched_signal_handler(int signum){
#ifdef SIGNALDBG
    cerr << "Parent: child launched signaled " << '\n';
#endif
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(&fork_mutex);
    signal_child=true;
    pthread_mutex_unlock(&fork_mutex);
#else
    signal_child=true;
#endif
  }
  /*
  void control_c(){
    if (ctrl_c){
      ctrl_c=false;
      interrupted=true;
      cerr << "Throwing exception" << '\n';
      throw(std::runtime_error("Stopped by Ctrl-C"));
    }
  }
  */

  // child sends a SIGUSR2 (intermediate data)
  void intermediate_signal_handler(int signum){
#ifdef SIGNALDBG
    cerr << "intermediate_signal_handler Parent called" << '\n';
#endif
    signal_plot_parent=true;
    child_busy=false;
    data_ready=true;
  }

  void child_signal_handler(int signum){
#ifdef SIGNALDBG
    cerr << "Child called" << '\n';
#endif
    signal_child=true;
  }
  
  void child_intermediate_done(int signum){
#ifdef SIGNALDBG
    cerr << "Child called for intermediate data" << '\n';
#endif
    signal_plot_child=true;
  }
  
  void kill_and_wait_sigusr2(){
    sigset_t mask, oldmask;
    sigemptyset (&mask);
    sigaddset (&mask, SIGUSR2);
    /* Wait for a signal to arrive. */
    sigprocmask (SIG_BLOCK, &mask, &oldmask);
    signal_plot_child=false;
#ifndef WIN32
    kill(parent_id,SIGUSR2);
#endif
    // cerr << "Child ready" << '\n';
#ifndef WIN32
    while (!signal_plot_child)
      sigsuspend (&oldmask);
    sigprocmask (SIG_UNBLOCK, &mask, NULL);
#endif
  }

  gen wait_parent(){
    kill_and_wait_sigusr2();
    ifstream child_in(cas_entree_name().c_str());    
    gen res;
    try {
      res=unarchive(child_in,context0);
    }
    catch (std::runtime_error & e){
      res=string2gen(e.what(),false);
    }
    child_in.close();
    return res;
  }

  static pid_t make_child(GIAC_CONTEXT){ // forks and return child id
    running_file=false;
    ctrl_c=false;
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(&fork_mutex);
    child_busy=false;
    pthread_mutex_unlock(&fork_mutex);
#else
    child_busy=false;
#endif
    signal(SIGINT,ctrl_c_signal_handler);
    signal(SIGUSR1,data_signal_handler);
    if (child_id>(pid_t) 1)
      return child_id; // exists
    signal_child=false;
    signal(SIGUSR2,child_launched_signal_handler); // don't do anything, just wait for child ready
    child_id=fork();
    if (child_id<(pid_t) 0)
      throw(std::runtime_error("Make_child error: Unable to fork"));
    if (child_id){ // parent process
#ifdef HAVE_LIBPTHREAD
      for (;;){
        pthread_mutex_lock(&fork_mutex);
        bool b=signal_child;
        pthread_mutex_unlock(&fork_mutex);
        if (b)
          break;
        usleep(1);
      }
#else
      signal_child=false;
      // parent process, wait child ready
      /* Wait for SIGUSR2. */
      while (!signal_child)
        usleep(1);
#endif

#ifdef SIGNALDBG
      cerr << "Parent received signal for child ready" << '\n';
#endif
      /* OK */
      signal(SIGUSR2,intermediate_signal_handler);
    } else {
#ifdef SIGNALDBG
      cerr << "Child launched" << '\n';
#endif
      // child process, redirect input/output
      sigset_t mask, oldmask;
      sigemptyset (&mask);
      sigaddset (&mask, SIGUSR1);
      signal(SIGUSR1,child_signal_handler);
      signal(SIGUSR2,child_intermediate_done);
      signal_child=false;
      gen args;
      /* Wait for a signal to arrive. */
      sigprocmask (SIG_BLOCK, &mask, &oldmask);
      kill(parent_id,SIGUSR2);
      signal_child=false;
      for (int no=0;;++no){
#ifdef SIGNALDBG
        cerr << "Child ready" << '\n';
#endif
#ifndef WIN32
	while (!signal_child)
	  sigsuspend (&oldmask);
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
#endif
#ifdef SIGNALDBG
        cerr << "Child reads and eval" << '\n';
#endif
	// read and evaluate input
	CLOCK_T start, end;
	double elapsed;
	start = CLOCK();
        string messages_to_print="";
	ifstream child_in(cas_entree_name().c_str());
	// Unarchive step
	try {
          args=unarchive(child_in,contextptr);
	}
	catch (std::runtime_error & error ){
	  last_evaled_argptr(contextptr)=NULL;
	  args = string2gen("Child unarchive error:"+string(error.what()),false);
	}
#ifdef SIGNALDBG
        cerr << "Child reads " << args << '\n';
#endif
	child_in.close();
        // Clone the context, so that we don't disturb anything
        context * ptr=clone_context(contextptr);
        gen args_evaled;
        if (ptr){
          try {
            args_evaled=args.eval(1,ptr);
          }
          catch (std::runtime_error & error){
            last_evaled_argptr(contextptr)=NULL;
            args_evaled=catch_err(error);
          }
          delete ptr;
        } else args_evaled=string2gen("Unable to clone context",false);
#ifdef SIGNALDBG
        cerr << "Child result " << args_evaled << '\n';
#endif
	block_signal=false;
	end = CLOCK();
	elapsed = ((double) (end - start)) / CLOCKS_PER_SEC;
	ofstream child_out(cas_sortie_name().c_str());
	archive(child_out,args,contextptr) ;
        int ta=taille(args_evaled,RAND_MAX);
        if (ta>=forkmaxleafsize){
          CERR << "Maxleafsize exceeded " << ta << ">=" << forkmaxleafsize << "\nYou can change maxleafsize by running fork_timeout(n) with a larger value of n\n";
          archive(child_out,undef,contextptr) ;
        }
        else
          archive(child_out,args_evaled,contextptr) ;
	child_out << messages_to_print ;
	int mm=messages_to_print.size();
	if (mm && (messages_to_print[mm-1]!='\n'))
	  child_out << '\n';
	child_out << "Time: " << elapsed << char(-65) ;
	child_out.close();
	// cerr << "Child sending signal to " << parent_id << '\n';
	/* Wait for a signal to arrive. */
	sigprocmask (SIG_BLOCK, &mask, &oldmask);
	signal_child=false;
#ifndef WIN32
#ifdef SIGNALDBG
        cerr << "Child sends SIGUSR1 to parent" << '\n';
#endif
	kill(parent_id,SIGUSR1);
#endif
      }
    }
#ifdef SIGNALDBG
    cerr << "Forked " << parent_id << " to " << child_id << '\n';
#endif
    return child_id;
  }

  static void archive_write_error(){
    cerr << "Archive error on " << cas_entree_name() << '\n';
  }
  
  // return true if entree has been sent to evalation by child process
  static bool child_eval(const string & entree,bool numeric,bool is_run_file,GIAC_CONTEXT){
#if defined(HAVE_NO_SIGNAL_H) || defined(DONT_FORK)
    return false;
#else
    if (is_run_file || rpn_mode(context0))
      history_begin_level=0;
    // added signal re-mapping because PARI seems to mess signal on the ipaq
    signal(SIGUSR1,data_signal_handler);
    signal(SIGUSR2,intermediate_signal_handler);
    if (!child_id)
      child_id=make_child(contextptr);
    if (child_busy || data_ready)
      return false;
    gen entr;
    CLOCK_T start, end;
    start = CLOCK();
    try {
      ofstream parent_out(cas_entree_name().c_str());
      if (!signal_plot_parent){
	parent_out << rpn_mode(context0) << " " << global_window_ymin << " " << history_begin_level << '\n';
	archive(parent_out,vecteur(history_in(context0).begin()+history_begin_level,history_in(context0).end()),context0);
	archive(parent_out,vecteur(history_out(context0).begin()+history_begin_level,history_out(context0).end()),context0);
      }
      if (is_run_file){
	ifstream infile(entree.c_str());
	char c;
	string s;
	while (!infile.eof()){
	  infile.get(c);
	  s += c;
	}
	entr = gen(s,context0);
	if (entr.type!=_VECT)
	  entr=gen(makevecteur(entr),_RUNFILE__VECT);
	else
	  entr.subtype=_RUNFILE__VECT;
      }
      else {
	entr = gen(entree,context0);
	if (numeric)
	  entr = symbolic(at_evalf,gen(entree,context0));
      }
      archive(parent_out,entr,context0);
      if (!parent_out)
	setsizeerr();
      parent_out.close();
      if (!parent_out)
	setsizeerr();
    } catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      archive_write_error();
      return false;
    }
    child_busy=true;
    if (signal_plot_parent){
      // cerr << "child_eval: Sending SIGUSR2 to child" << '\n';
      signal_plot_parent=false;
#ifndef WIN32
      kill(child_id,SIGUSR2);
#endif
      return true;
    }
    // cerr << "Sending SIGUSR1 to " << child_id << '\n';
#ifndef WIN32
    kill(child_id,SIGUSR1);
#endif
    running_file=is_run_file;
    end = CLOCK();
    // cerr << "# Save time" << double(end-start)/CLOCKS_PER_SEC << '\n';
    return true;
#endif /// HAVE_NO_SIGNAL_H
  }

  static bool child_reeval(int history_begin_level,GIAC_CONTEXT){
#if defined(HAVE_NO_SIGNAL_H) || defined(DONT_FORK)
    return false;
#else
    signal(SIGUSR1,data_signal_handler);
    signal(SIGUSR2,intermediate_signal_handler);
    if (!child_id)
      child_id=make_child(contextptr);
    if (child_busy || data_ready)
      return false;
    string messages_to_print="";
    try {
      ofstream parent_out(cas_entree_name().c_str());
      parent_out << rpn_mode(context0) << " " << global_window_ymin << " " << -1-history_begin_level << " " << synchronize_history << '\n';
      if (synchronize_history){
	archive(parent_out,history_in(context0),context0);
	archive(parent_out,vecteur(history_out(context0).begin(),history_out(context0).begin()+history_begin_level),context0);
      }
      else
	archive(parent_out,history_in(context0)[history_begin_level],context0);
      if (!parent_out)
	setsizeerr();
      parent_out.close();
      if (!parent_out)
	setsizeerr();
    } catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      archive_write_error();
      return false;
    }
    child_busy=true;
    running_file=true;
    // erase the part of the history that we are computing again
    if (run_modif_pos<signed(history_in(context0).size()))
      history_in(context0).erase(history_in(context0).begin()+run_modif_pos,history_in(context0).end());
    if (run_modif_pos<signed(history_out(context0).size()))
      history_out(context0).erase(history_out(context0).begin()+run_modif_pos,history_out(context0).end());
#ifndef WIN32
    kill(child_id,SIGUSR1);
#endif
    return true;
#endif /// HAVE_NO_SIGNAL_H
  }

  static void updatePICT(const vecteur & args){
    const_iterateur it=args.begin(),itend=args.end();
    gen sortie;
    for (;it!=itend;++it){
      sortie=*it;
      if (sortie.type==_POINTER_ && sortie.subtype==_FL_WIDGET_POINTER && fl_widget_updatepict_function){
	sortie = fl_widget_updatepict_function(sortie);
	// cerr << "updatepict" << sortie << '\n';
      }
      if ( (sortie.type==_SYMB) && equalposcomp(plot_sommets,sortie._SYMBptr->sommet)){
#ifdef WITH_GNUPLOT
	plot_instructions.push_back(sortie);
#endif
	if ((sortie._SYMBptr->feuille.type==_VECT) && (sortie._SYMBptr->feuille._VECTptr->size()==3) && (sortie._SYMBptr->feuille._VECTptr->back().type==_STRNG) && ( ((*sortie._SYMBptr->feuille._VECTptr)[1].type==_VECT) || is_zero((*sortie._SYMBptr->feuille._VECTptr)[1])) ){
	  string lab=*(sortie._SYMBptr->feuille._VECTptr->back()._STRNGptr);
#ifdef WITH_GNUPLOT
	  if (lab.size() && (lab>=PICTautoname)){
	    PICTautoname=lab;
	    PICTautoname_plus_plus();
	  }
#endif
	}
      }
      else {
	if ( ((sortie.type==_SYMB) && (sortie._SYMBptr->sommet==at_erase)) ||
             ((sortie.type==_FUNC) && (*sortie._FUNCptr==at_erase)) ){
#ifdef WITH_GNUPLOT
	  plot_instructions.clear();
#endif
	}
	else {
	  if ( (sortie.type==_VECT) && (sortie._VECTptr->size()) && (sortie._VECTptr->back().type==_SYMB) && (equalposcomp(plot_sommets,sortie._VECTptr->back()._SYMBptr->sommet))){
#ifdef WITH_GNUPLOT
	    plot_instructions.push_back(sortie);
#endif
	    sortie=sortie._VECTptr->back();
	    if ((sortie._SYMBptr->feuille.type==_VECT) && (sortie._SYMBptr->feuille._VECTptr->size()==3) && (sortie._SYMBptr->feuille._VECTptr->back().type==_STRNG)  ){
	      string lab=*(sortie._SYMBptr->feuille._VECTptr->back()._STRNGptr);
#ifdef WITH_GNUPLOT
	      if (lab.size() && (lab>=PICTautoname)){
		PICTautoname=lab;
		PICTautoname_plus_plus();
	      }
#endif
	    } 
	  }
	  else {
#ifdef WITH_GNUPLOT
	    plot_instructions.push_back(zero);
#endif
	  }
	}
      }
    } // end for (;it!=itend;++it)
  } 

  static void signal_child_ok(){
    child_busy=true;
    data_ready=false;
    signal_plot_parent=false;
#ifndef WIN32
    kill(child_id,SIGUSR2);
#endif // WIN32
  }

  static const unary_function_eval * parent_evalonly_sommets_alias[]={*(const unary_function_eval **) &at_widget_size,*(const unary_function_eval **) &at_keyboard,*(const unary_function_eval **) &at_current_sheet,*(const unary_function_eval **) &at_Row,*(const unary_function_eval **) &at_Col,0};
  static const unary_function_ptr * parent_evalonly_sommets=(const unary_function_ptr *) parent_evalonly_sommets_alias;
  static bool update_data(gen & entree,gen & sortie,GIAC_CONTEXT){
    // if (entree.type==_IDNT)
    //   entree=symbolic(at_sto,makevecteur(sortie,entree));
    // discarded sto autoadd otherwise files with many definitions
    // are overwritten
    debug_ptr(contextptr)->debug_mode=false;
    if (signal_plot_parent){
      // cerr << "Child signaled " << entree << " " << sortie << '\n';
      if ( entree.type==_SYMB ){
	if ( (entree._SYMBptr->sommet==at_click && entree._SYMBptr->feuille.type==_VECT && entree._SYMBptr->feuille._VECTptr->empty() ) 
	     || (entree._SYMBptr->sommet==at_debug) 
	     ) {
	  debug_ptr(contextptr)->debug_mode=(entree._SYMBptr->sommet==at_debug);
	  // cerr << "Child waiting" << '\n';
	  data_ready=false;
	  *debug_ptr(contextptr)->debug_info_ptr=entree._SYMBptr->feuille;
	  debug_ptr(contextptr)->debug_refresh=true;
	  return true;
	}
	if ( entree._SYMBptr->sommet==at_click || entree._SYMBptr->sommet==at_inputform || entree._SYMBptr->sommet==at_interactive ){
	  // cerr << entree << '\n';
	  gen res=entree.eval(1,contextptr);
	  // cerr << res << '\n';
	  ofstream parent_out(cas_entree_name().c_str());
	  archive(parent_out,res,contextptr);
	  parent_out.close();
	  signal_child_ok();
	  return true;
	}
	// cerr << "Child signaled " << entree << " " << sortie << '\n';
      }
      if (sortie.type==_SYMB){
	if (sortie._SYMBptr->sommet==at_SetFold){
	  current_folder_name=sortie._SYMBptr->feuille;
	  signal_child_ok();
	  return false;
	}
	if (sortie._SYMBptr->sommet==at_sto && sortie._SYMBptr->feuille.type==_VECT){
	  vecteur & v=*sortie._SYMBptr->feuille._VECTptr;
	  // cerr << v << '\n';
	  if ((v.size()==2) && v[1].type==_IDNT && v[1]._IDNTptr->ref_count_ptr!=(int*)-1){
	    if (v[1]._IDNTptr->value)
	      delete v[1]._IDNTptr->value;
	    v[1]._IDNTptr->value = new gen(v[0]);
	  }
	  signal_child_ok();
	  return false;	  
	}
	if (sortie._SYMBptr->sommet==at_purge){
	  gen & g=sortie._SYMBptr->feuille;
	  if (g.type==_IDNT && (g._IDNTptr->value) && g._IDNTptr->ref_count_ptr!=(int *) -1){
	    delete g._IDNTptr->value;
	    g._IDNTptr->value=0;
	  }
	  signal_child_ok();
	  return false;
	}
	if ((sortie._SYMBptr->sommet==at_cd) && (sortie._SYMBptr->feuille.type==_STRNG)){
#ifndef HAVE_NO_CWD
	  chdir(sortie._SYMBptr->feuille._STRNGptr->c_str());
#endif
	  signal_child_ok();
	  return false;
	}
	if ( sortie._SYMBptr->sommet==at_insmod || sortie._SYMBptr->sommet==at_rmmod || sortie._SYMBptr->sommet==at_user_operator ){
	  protecteval(sortie,DEFAULT_EVAL_LEVEL,contextptr);
	  signal_child_ok();
	  return false;
	}
	if (sortie._SYMBptr->sommet==at_xyztrange){
	  gen f=sortie._SYMBptr->feuille;
	  if ( (f.type==_VECT) && (f._VECTptr->size()>=12)){
	    protecteval(sortie,2,contextptr);
            signal_child_ok();
	    return false;
	  }
	}
	if (sortie._SYMBptr->sommet==at_cas_setup){
	  gen f=sortie._SYMBptr->feuille;
	  if ( (f.type==_VECT) && (f._VECTptr->size()>=7)){
	    vecteur v=*f._VECTptr;
	    cas_setup(v,contextptr); 
            signal_child_ok();
	    return false;
	  }
	}
      }
#if 0
      if (entree.type==_SYMB && entree._SYMBptr->sommet==at_signal && sortie.type==_SYMB && equalposcomp(parent_evalonly_sommets,sortie._SYMBptr->sommet) ) {
	gen res=sortie.eval(1,contextptr);
	ofstream parent_out(cas_entree_name().c_str());
	archive(parent_out,res,contextptr);
	parent_out.close();
	signal_child_ok();	
	return false;
      }
#endif
    } // end signal_plot_parent
    // cerr << "# Parse time" << double(end-start)/CLOCKS_PER_SEC << '\n';
    // see if it's a PICT update
    vecteur args;
    // update history
    if (rpn_mode(contextptr)) {
      if ((sortie.type==_VECT)&& (sortie.subtype==_RPN_STACK__VECT)){
	history_out(contextptr)=*sortie._VECTptr;
	history_in(contextptr)=vecteur(history_out(contextptr).size(),undef);
	int i=erase_pos(contextptr);
	args=vecteur(history_out(contextptr).begin()+i,history_out(contextptr).end());
#ifdef WITH_GNUPLOT
	plot_instructions.clear();
#endif
      }
      else {
	if (entree.type==_FUNC){
	  int s=giacmin(giacmax(entree.subtype,0),(int)history_out(contextptr).size());
	  vecteur v(s);
	  for (int k=s-1;k>=0;--k){
	    v[k]=history_out(contextptr).back();
	    history_out(contextptr).pop_back();
	    history_in(contextptr).pop_back();
	  }
	  entree=symbolic(*entree._FUNCptr,v);
	}
	history_in(contextptr).push_back(entree);
	history_out(contextptr).push_back(sortie);
	int i=erase_pos(contextptr);
	args=vecteur(history_out(contextptr).begin()+i,history_out(contextptr).end());
#ifdef WITH_GNUPLOT
	plot_instructions.clear();
#endif
      }
    }  
    else {
      bool fait=false;
      if (running_file) {
	if (entree.type==_VECT && sortie.type==_VECT) {
	  history_in(contextptr)=mergevecteur(history_in(contextptr),*entree._VECTptr);
	  history_out(contextptr)=mergevecteur(history_out(contextptr),*sortie._VECTptr);
	  fait=true;
	}
	if (is_zero(entree) && is_zero(sortie))
	  fait=true;
      }
      if (!fait){
	if (in_texmacs){
	  COUT << GIAC_DATA_BEGIN << "verbatim:";
	  COUT << "ans(" << history_out(contextptr).size() << ") " << sortie << "\n";
  
	  COUT << GIAC_DATA_BEGIN << "latex:$$ " << gen2tex(entree,contextptr) << "\\quad = \\quad " << gen2tex(sortie,contextptr) << "$$" << GIAC_DATA_END;
	  COUT << "\n";
	  COUT << GIAC_DATA_BEGIN << "channel:prompt" << GIAC_DATA_END;
          COUT << "quest(" << history_out(contextptr).size()+1 << ") ";
          COUT << GIAC_DATA_END;
          fflush (stdout);
	}
	history_in(contextptr).push_back(entree);
	history_out(contextptr).push_back(sortie);
	// for PICT update
	args=vecteur(1,sortie);
      }
      if (running_file){
	// for PICT update
	int i=erase_pos(contextptr);
	args=vecteur(history_out(contextptr).begin()+i,history_out(contextptr).end());
	// CERR << "PICT clear" << '\n';
#ifdef WITH_GNUPLOT
	plot_instructions.clear();
#endif
        //running_file=false;
      }
      // now do the update
    }
    updatePICT(args);
    data_ready=false;
    if (signal_plot_parent)
        signal_child_ok();
    return true;
  }

  static void archive_read_error(){
    CERR << "Read error on " << cas_sortie_name() << '\n';
    data_ready=false;
#ifndef WIN32
    if (child_id)
      kill(child_id,SIGKILL);
#endif
    child_id=0;
  }

  static bool read_data(gen & entree,gen & sortie,string & message,GIAC_CONTEXT){
    if (!data_ready)
      return false;
    message="";
    try {
      ifstream parent_in(cas_sortie_name().c_str());
      if (!parent_in)
	setsizeerr();
      CLOCK_T start, end;  
      start = CLOCK();
      entree=unarchive(parent_in,contextptr);
      sortie=unarchive(parent_in,contextptr);
      end = CLOCK();
      parent_in.getline(buf,BUFFER_SIZE,char(-65));
      if (buf[0]=='\n')
	message += (buf+1);
      else
	message += buf;
      if (!parent_in)
	setsizeerr();
    } catch (std::runtime_error & ){
      last_evaled_argptr(contextptr)=NULL;
      archive_read_error();
      return false;
    }
    return update_data(entree,sortie,contextptr);
  }

  gen _fork_timeout(const gen & args,GIAC_CONTEXT){
    signal(SIGUSR1,SIG_IGN);    
    // cerr << "fork_timeout step 1\n";
    if (args.type==_INT_){
      int n=args.val;
      forkmaxleafsize=giacmin(giacmax(n,16),65536);
      return forkmaxleafsize;
    }
    if (args.type==_VECT && args._VECTptr->empty())
      return forkmaxleafsize;
    if (args.type!=_VECT || args._VECTptr->size()<2)
      return gensizeerr(contextptr);
    gen entr=args._VECTptr->front();
    int ta=taille(entr,RAND_MAX);
    if (ta>=forkmaxleafsize){
      CERR << "Maxleafsize exceeded " << ta << ">=" << forkmaxleafsize << "\nYou can change maxleafsize by running fork_timeout(n) with a larger value of n\n";
      return undef;
    }
    // cerr << "fork_timeout step 2\n";
    gen tout=evalf((*args._VECTptr)[1],1,contextptr);
    bool killchild=false;
    // fork_timeout(expression,dt,1) will not kill the child if it already exists, this is faster *but* the context/variables from parent are not copied
    if (args._VECTptr->size()==3)
      killchild=is_zero(args._VECTptr->back());
    if (tout.type!=_DOUBLE_)
      return gensizeerr(contextptr);
    double dt=tout._DOUBLE_val;
    if (dt<1e-3)
      return gensizeerr("Invalid timeout, should be at least 1e-3");
    // fork every time now, maybe improved by sending context to child
    if (killchild || child_busy || data_ready){
      if (child_id>1){
        kill(child_id,SIGKILL);
        usleep(1);
      }
      child_id=1;
      child_busy=data_ready=false;
    }
    // cerr << "fork_timeout step 3\n";
    if (child_id<=1)
      child_id=make_child(contextptr);
    // signal(SIGUSR2,intermediate_signal_handler);
    // cerr << "fork_timeout step 4\n";
    try {
      ofstream parent_out(cas_entree_name().c_str());
      archive(parent_out,entr,contextptr);
      if (!parent_out)
	setsizeerr();
      parent_out.close();
      if (!parent_out)
	setsizeerr();
    } catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      archive_write_error();
      return gensizeerr("Fork_timeout; archive write error");
    }
    // cerr << "fork_timeout step 5\n";
    CLOCK_T start, end;
    start = CLOCK();
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&fork_mutex);
    child_busy=true;
    pthread_mutex_unlock(&fork_mutex);
#else
    child_busy=true;
#endif
    signal(SIGUSR1,data_signal_handler);    
#ifdef SIGNALDBG
    cerr << "Sending SIGUSR1 to " << child_id << '\n';
#endif
    kill(child_id,SIGUSR1);
    // now wait for timeout or signal
    gen g_in=entr,g_out; string msg; int N=dt/1e-3;
    double debut=realtime();
    for (;;){
#if 0 // def SIGNALDBG
      CERR << "data_ready " << data_ready << " child_busy " << child_busy << "\n";
#endif
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_lock(&fork_mutex);
      bool b=!data_ready || child_busy;
      pthread_mutex_unlock(&fork_mutex);
#else
      bool b=!data_ready || child_busy;
#endif
      if (b){
        usleep(1);
        double cur=realtime()-debut;
#if 0 // def SIGNALDBG
        CERR << "Waiting for " << cur << "\n";
#endif
        if (cur>dt){
          CERR << "Timeout\n";
          kill(child_id,SIGKILL);
          usleep(10);
          child_id=1;
          g_out=string2gen("timeout",false);
#ifdef HAVE_LIBPTHREAD
          pthread_mutex_lock(&fork_mutex);
          child_busy=data_ready=false;
          pthread_mutex_unlock(&fork_mutex);
#else
          child_busy=data_ready=false;
#endif
          break;
        }
        continue;
      }
#ifdef SIGNALDBG
      CERR << "Data ready\n";
#endif
      string message="";
      try {
        ifstream parent_in(cas_sortie_name().c_str());
        if (!parent_in)
          setsizeerr();
        g_in=unarchive(parent_in,contextptr);
        g_out=unarchive(parent_in,contextptr);
        parent_in.getline(buf,BUFFER_SIZE,char(-65));
        if (buf[0]=='\n')
          message += (buf+1);
        else
          message += buf;
        if (!parent_in)
          setsizeerr();
        // FIXME: at the end of icas, remove \#cas*
      } catch (std::runtime_error & err){
        last_evaled_argptr(contextptr)=NULL;
        archive_read_error();
        g_out=string2gen(err.what(),false);
      }
      break;
    }
    // cerr << "# Save time" << double(end-start)/CLOCKS_PER_SEC << '\n';
    return g_out;
  }
  static const char _fork_timeout_s []="fork_timeout";
  static define_unary_function_eval_quoted (__fork_timeout,&_fork_timeout,_fork_timeout_s);
  define_unary_function_ptr5( at_fork_timeout ,alias_at_fork_timeout,&__fork_timeout,_QUOTE_ARGUMENTS,true);
  
#endif // HAVE_SIGNAL_H_OLD || HAVE_SIGNAL_H

  string home_directory(){
    string s("/");
#ifdef FXCG
    return s;
#else
    if (getenv("GIAC_HOME"))
      s=getenv("GIAC_HOME");
    else {
      if (getenv("XCAS_HOME"))
	s=getenv("XCAS_HOME");
    }
    if (!s.empty() && s[s.size()-1]!='/')
      s += '/';
    if (s.size()!=1)
      return s;
#ifdef HAVE_NO_HOME_DIRECTORY
    return s;
#else
    if (access("/etc/passwd",R_OK))
      return "";
    uid_t u=getuid();
    passwd * p=getpwuid(u);
    if (p) s=p->pw_dir;
    return s+"/";
#endif
#endif
  }

#ifndef FXCG

#if defined HAVE_SYS_TYPES_H && defined HAVE_UNISTD_H
  string tmpfs(){
    static string tmpfs="";
    if (tmpfs.size()==0){
      int id=getuid();
      if (id>0){ // Debian tmpfs
        tmpfs="/run/user/"+print_INT_(id)+"/";
        if (!is_file_available(tmpfs.c_str()))
          tmpfs="/tmp/";
      }
      else
        tmpfs="/tmp/";
    }
    return tmpfs;    
  }
#else
  string tmpfs(){
    return "/tmp/";
  }
#endif

  string cas_entree_name(){
    if (getenv("XCAS_TMP"))
      return getenv("XCAS_TMP")+("/#cas_entree#"+print_INT_(parent_id));
    string tmp=tmpfs();
    if (tmp=="/tmp/")
      tmp=home_directory();
    return tmp+"#cas_entree#"+print_INT_(parent_id);
  }

  string cas_sortie_name(){
    if (getenv("XCAS_TMP"))
      return getenv("XCAS_TMP")+("/#cas_sortie#"+print_INT_(parent_id));
    string tmp=tmpfs();
    if (tmp=="/tmp/")
      tmp=home_directory();
    return tmp+"#cas_sortie#"+print_INT_(parent_id);
  }
#endif
  
  void read_config(const string & name,GIAC_CONTEXT,bool verbose){
#if !defined NSPIRE && !defined FXCG && !defined GIAC_HAS_STO_38
#if !defined __MINGW_H 
    if (access(name.c_str(),R_OK)) {
      if (verbose)
	CERR << "// Unable to find config file " << name << '\n';
      return;
    }
#endif
    ifstream inf(name.c_str());
    if (!inf)
      return;
    vecteur args;
    if (verbose)
      CERR << "// Reading config file " << name << '\n';
    readargs_from_stream(inf,args,contextptr);
    gen g(args);
    if (debug_infolevel || verbose)    
      CERR << g << '\n';
    g.eval(1,contextptr);
    if (verbose){
      CERR << "// User configuration done" << '\n';
      CERR << "// Maximum number of parallel threads " << threads << '\n';
      CERR << "Threads allowed " << threads_allowed << '\n';
    }
    if (debug_infolevel){
#ifdef HASH_MAP_NAMESPACE
      CERR << "Using hash_map_namespace"<< '\n';
#endif
      CERR << "Mpz_class allowed " << mpzclass_allowed << '\n';
      // CERR << "Heap multiplication " << heap_mult << '\n';
    }
#endif
  }

  // Unix: configuration is read from xcas.rc in the giac_aide_location dir
  // then from the user ~/.xcasrc
  // Win: configuration from $XCAS_ROOT/xcas.rc then from home_dir()+xcasrc
  // or if not available from current dir xcasrc
  void protected_read_config(GIAC_CONTEXT,bool verbose){
#ifndef NO_STDEXCEPT
    try {
#endif
      string s;
#ifdef WIN32
      s=home_directory();
#ifdef GNUWINCE
	  s = xcasroot();
#else
      if (s.size()<2 && getenv("XCAS_ROOT")){
	s=getenv("XCAS_ROOT");
	if (debug_infolevel || verbose)
	  CERR << "Found XCAS_ROOT " << s << '\n';
      }
#endif // GNUWINCE
#else
      s=giac_aide_location;
      s=s.substr(0,s.size()-8);
#endif
      if (s.size()){
        if (s[s.size()-1]=='/')
          read_config(s+"xcas.rc",contextptr,verbose);
        else
          read_config(s+"/xcas.rc",contextptr,verbose);
      }
      s=home_directory();
      if (s.size()<2)
	s="";
      read_config(s+xcasrc(),contextptr,verbose);
#ifndef NO_STDEXCEPT
    }
    catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      CERR << "Error in config file " << xcasrc() << " " << e.what() << '\n';
    }
#endif
  }

  string giac_aide_dir(){
#if defined NSPIRE || defined FXCG || defined MINGW32
    return xcasroot();
#else
    if (!access((xcasroot()+"aide_cas").c_str(),R_OK)){
      return xcasroot();
    }
    if (getenv("XCAS_ROOT")){
      string s=getenv("XCAS_ROOT");
      return s;
    }
    if (xcasroot().size()>4 && xcasroot().substr(xcasroot().size()-4,4)=="bin/"){
      string s(xcasroot().substr(0,xcasroot().size()-4));
      s+="share/giac/";
      if (!access((s+"aide_cas").c_str(),R_OK)){
	return s;
      }
    }
#ifdef __APPLE__
    if (!access("/Applications/usr/share/giac/",R_OK))
      return "/Applications/usr/share/giac/";
    return "/Applications/usr/share/giac/";
#endif
#if defined WIN32 // check for default install path
#ifdef MINGW
    string ns("c:\\xcaswin\\");
#else
    string ns("/cygdrive/c/xcas/");
#endif
    if (!access((ns+"aide_cas").c_str(),R_OK)){
      CERR << "// Giac share root-directory:" << ns << '\n';
      return ns;
    }
#endif // WIN32
    string s(giac_aide_location); // ".../aide_cas"
    // test if aide_cas is there, if not test at xcasroot() return ""
    if (!access(s.c_str(),R_OK)){
      s=s.substr(0,s.size()-8);
      CERR << "// Giac share root-directory:" << s << '\n';
      return s;
    }
    return "";
#endif // __MINGW_H
  }

  std::string absolute_path(const std::string & orig_file){
#ifdef BESTA_OS 
    // BP: FIXME
    return orig_file;
#else
#if (!defined WIN32) || (defined VISUALC)
    if (orig_file[0]=='/')
      return orig_file;
    else
      return giac_aide_dir()+orig_file;
#else
#if !defined GNUWINCE && !defined __MINGW_H
     string res=orig_file;
     const char *_epath;
     _epath = orig_file.c_str()  ;
     /* If we have a POSIX path list, convert to win32 path list */
     if (_epath != NULL && *_epath != 0
         && cygwin_posix_path_list_p (_epath)){
#ifdef x86_64
       int s = cygwin_conv_path (CCP_POSIX_TO_WIN_A , _epath, NULL, 0);
       char * _win32path = (char *) malloc(s);
       cygwin_conv_path(CCP_POSIX_TO_WIN_A,_epath, _win32path,s);
       s=strlen(_win32path);
#else
       char * _win32path = (char *) malloc
	 (cygwin_posix_to_win32_path_list_buf_size (_epath));
       cygwin_posix_to_win32_path_list (_epath, _win32path);
       int s=strlen(_win32path);
#endif
       res.clear();
       for (int i=0;i<s;++i){
	 char ch=_win32path[i];
	 if (ch=='\\' || ch==' ')
	   res += '\\';
	 res += ch;
       }
       free(_win32path);
     }
     return res;
#else
    string file=orig_file,s;
    if (file[0]!='/'){
      file=giac_aide_dir()+file;
    }
    if (file.substr(0,10)=="/cygdrive/" || (file[0]!='/' && file[1]!=':')){
      string s1=xcasroot();
      if (file.substr(0,10)=="/cygdrive/")
	s1=file[10]+(":"+file.substr(11,file.size()-11));
      else {
	// remove /cygdrive/
	if (s1.substr(0,10)=="/cygdrive/")
	  s1=s1[10]+(":"+s1.substr(11,s1.size()-11));
	else
	  s1="c:/xcas/";
	s1 += file;
      }
      string s2;
      int t=s1.size();
      for (int i=0;i<t;++i){
	if (s1[i]=='/')
	  s2+="\\\\";
	else {
	  if (s1[i]==' ')
	    s2+='\\';
	  s2+=s1[i];
	}
      }
      return s2;
    }
#endif // GNUWINCE
#endif // WIN32
    return orig_file;
#endif // BESTA_OS
  }

  bool is_file_available(const char * ch){
    if (!ch)
      return false;
#if !defined NSPIRE && !defined FXCG 
#if defined MINGW32 && defined GIAC_GGB
    return true;
#else
    FILE * f =fopen(ch,"r");
    if (f){
      fclose(f);
      return true;
    }
    return false;
    //if (access(ch,R_OK)) return false;
#endif
#endif
    return false;
  }

  bool file_not_available(const char * ch){
    return !is_file_available(ch);
  }

  static void add_slash(string & path){
    if (!path.empty() && path[path.size()-1]!='/')
      path += '/';
  }
#ifdef FXCG
  const char * getenv(const char *){
    return "";
  }
#endif

  bool check_file_path(const string & s){
    int ss=int(s.size()),i;
    for (i=0;i<ss;++i){
      if (s[i]==' ')
	break;
    }
    string name=s.substr(0,i);
#ifdef FXCG
    const char ch[]="/";
#else
    const char * ch=getenv("PATH");
#endif
    if (!ch || name[0]=='/')
      return is_file_available(name.c_str());
    string path;
    int l=int(strlen(ch));
    for (i=0;i<l;++i){
      if (ch[i]==':'){
	if (!path.empty()){
	  add_slash(path);
	  if (is_file_available((path+name).c_str()))
	    return true;
	}
	path="";
      }
      else
	path += ch[i];
    }
    add_slash(path);
    return path.empty()?false:is_file_available((path+name).c_str());
  }

  string browser_command(const string & orig_file){
#if defined NSPIRE || defined FXCG || defined MINGW32
    return "";
#else
    string file=orig_file;
    string s;
    bool url=false;
    if (file.size()>=4 && file.substr(0,4)=="http" || file.substr(0,4)=="mail"){
      url=true;
      s="'"+file+"'";
    }
    else {
      if (file[0]!='/'){
#ifdef WIN32
	file=giac_aide_dir()+file;
#else    
	s=giac_aide_dir();
#endif
      }
      s="file:"+s+file;
    }
    if (debug_infolevel)
      CERR << s << '\n';
#ifdef WIN32
    bool with_firefox=false;
    /*
    string firefox="/cygdrive/c/Program Files/Mozilla Firefox/firefox.exe";
    if (getenv("BROWSER")){
      string tmp=getenv("BROWSER");
      if (tmp=="firefox" || tmp=="mozilla"){
	with_firefox=!access(firefox.c_str(),R_OK);
	if (!with_firefox){
	  firefox="/cygdrive/c/Program Files/mozilla.org/Mozilla/mozilla.exe";
	  with_firefox=!access(firefox.c_str(),R_OK);
	}
      }
    }
    */
    if (!url && (file.substr(0,10)=="/cygdrive/" || (file[0]!='/' && file[1]!=':')) ){
      string s1=xcasroot();
      if (file.substr(0,10)=="/cygdrive/")
	s1=file[10]+(":"+file.substr(11,file.size()-11));
      else {
	// remove /cygdrive/
	if (s1.substr(0,10)=="/cygdrive/")
	  s1=s1[10]+(":"+s1.substr(11,s1.size()-11));
	else
	  s1="c:/xcas/";
	s1 += s.substr(5,s.size()-5);
      }
      CERR << "s1=" << s1 << '\n';
      string s2;
      if (with_firefox)
	s2=s1;
      else {
	int t=int(s1.size());
	for (int i=0;i<t;++i){
	  if (s1[i]=='/')
	    s2+="\\";
	  else
	    s2+=s1[i];
	}
      }
      CERR << "s2=" << s2 << '\n';
      s=s2;
      // s="file:"+s2;
      // s = s.substr(0,5)+"C:\\\\xcas\\\\"+s2;
    }
    // Remove # trailing part of URL
    int ss=int(s.size());
    for (--ss;ss>0;--ss){
      if (s[ss]=='#' || s[ss]=='.' || s[ss]=='/' )
	break;
    }
    if (ss && s[ss]!='.')
      s=s.substr(0,ss);
    s=xcasroot()+"cygstart.exe '"+s+"' &";
    /*
    if (with_firefox){
      s="'"+firefox+"' '"+s+"' &";
    }
    else {
      if (getenv("BROWSER"))
	s=getenv("BROWSER")+(" '"+s+"' &");
      else 
	s="'/cygdrive/c/Program Files/Internet Explorer/IEXPLORE.EXE' '"+s+"' &";
    }
    */
#else
    string browser;
    if (getenv("BROWSER"))
      browser=getenv("BROWSER");
    else {
#ifdef __APPLE__
      browser="open" ; // browser="/Applications/Safari.app/Contents/MacOS/Safari";
      // Remove file: that seems not supported by Safari
      if (!url)
	s = s.substr(5,s.size()-5);
      // Remove # trailing part of URL
      int ss=s.size();
      for (--ss;ss>0;--ss){
	if (s[ss]=='#' || s[ss]=='.' || s[ss]=='/' )
	  break;
      }
      if (ss && s[ss]!='.')
	s=s.substr(0,ss);
#else
      browser="mozilla";
      if (!access("/usr/bin/dillo",R_OK))
	browser="dillo";
      if (!access("/usr/bin/xdg-open",R_OK))
        browser="xdg-open";
      if (!access("/usr/bin/chromium",R_OK))
	browser="chromium";
      if (!access("/usr/bin/firefox",R_OK))
	browser="firefox";
      if (!access("/usr/bin/open",R_OK))
	browser="open";
#endif
    }
    // find binary name
    int bs=browser.size(),i;
    for (i=bs-1;i>=0;--i){
      if (browser[i]=='/')
	break;
    }
    ++i;
    string browsersub=browser.substr(i,bs-i);
    if (s[0]!='\'') s='\''+s+'\'';
    if (browsersub=="mozilla" || browsersub=="mozilla-bin"
        //|| browsersub=="firefox"
        || browsersub=="chromium"){
      s="if ! "+browser+" -remote \"openurl("+s+")\" ; then "+browser+" "+s+" & fi &";
    }
    else
      s=browser+" "+s+" &";
#endif
    //if (debug_infolevel)
      CERR << "// Running command:"+ s<<'\n';
    return s;
#endif // __MINGW_H
  }

  bool system_browser_command(const string & file){
#ifdef EMCC2
    EM_ASM_ARGS({
        var url=UTF8ToString($0);
        console.log('system_browser_command',url);
        window.open(url, '_blank').focus();
      },file.c_str());
    return true;
#endif
#if defined BESTA_OS || defined POCKETCAS
    return false;
#else
#ifdef WIN32
    string res=file;
    if (file.size()>4 && file.substr(0,4)!="http" && file.substr(0,4)!="file" && file.substr(0,4)!="mail"){
      if (res[0]!='/')
	res=giac_aide_dir()+res;
      if (file.substr(0,4)!="xcas" && file.substr(0,8)!="doc/xcas"){
        // Remove # trailing part of URL
        int ss=int(res.size());
        for (--ss;ss>0;--ss){
          if (res[ss]=='#' || res[ss]=='.' || res[ss]=='/' )
            break;
        }
        if (ss && res[ss]!='.')
          res=res.substr(0,ss);
      }
      CERR << res << '\n';
#if !defined VISUALC && !defined __MINGW_H && !defined NSPIRE && !defined FXCG
      /* If we have a POSIX path list, convert to win32 path list */
      const char *_epath;
      _epath = res.c_str()  ;
      if (_epath != NULL && *_epath != 0
	  && cygwin_posix_path_list_p (_epath)){
#ifdef x86_64
	int s = cygwin_conv_path (CCP_POSIX_TO_WIN_A , _epath, NULL, 0);
	char * _win32path = (char *) malloc(s);
	cygwin_conv_path(CCP_POSIX_TO_WIN_A,_epath, _win32path,s);
#else
	char * _win32path = (char *) malloc (cygwin_posix_to_win32_path_list_buf_size (_epath));
	cygwin_posix_to_win32_path_list (_epath, _win32path);
#endif
	res = _win32path;
	free(_win32path);
      }
#endif
    }
    CERR << res << '\n';
#if !defined VISUALC && !defined NSPIRE && !defined FXCG
#ifdef __MINGW_H
    while (res.size()>=2 && res.substr(0,2)=="./")
      res=res.substr(2,res.size()-2);
    if (res.size()<4 || (res.substr(0,4)!="http" && res.substr(0,4)!="mail"))
      res = "file:///c:/xcaswin/"+res;
    CERR << "running open on " << res << '\n';
    //ShellExecute(NULL,"open","file:///c:/xcaswin/doc/fr/cascmd_fr/index.html",\
NULL,NULL,SW_SHOWNORMAL);
    ShellExecute(NULL,"open",res.c_str(),NULL,NULL,SW_SHOWNORMAL);
#else
    // FIXME: works under visualc but not using /UNICODE flag
    // find correct flag
    ShellExecute(NULL,NULL,res.c_str(),NULL,NULL,1);
#endif
#endif
    return true;
#else
#ifdef BESTA_OS
    return false; // return 1;
#else
    return !system_no_deprecation(browser_command(file).c_str());
#endif
#endif
#endif
  }

  vecteur remove_multiples(vecteur & ww){
    vecteur w;
    if (!ww.empty()){
      islesscomplexthanf_sort(ww.begin(),ww.end());
      gen prec=ww[0];
      for (unsigned i=1;i<ww.size();++i){
	if (ww[i]==prec)
	  continue;
	w.push_back(prec);
	prec=ww[i];
      }
      w.push_back(prec);
    }
    return w;
  }

  int equalposcomp(const vector<int> v,int i){
    vector<int>::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      if (*it==i)
	return int(it-v.begin())+1;
    return 0;
  }

  int equalposcomp(const vector<short int> v,int i){
    vector<short int>::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      if (*it==i)
	return int(it-v.begin())+1;
    return 0;
  }

  int equalposcomp(int tab[],int f){
    for (int i=1;*tab!=0;++tab,++i){
      if (*tab==f)
	return i;
    }
    return 0;
  }

  std::string find_lang_prefix(int i){
    switch (i){
    case 1:
      return "fr/";
    case 2:
      return "en/";
    case 3:
      return "es/";
    case 4:
      return "el/";
    case 9:
      return "pt/";
    case 6:
      return "it/";
      /*
    case 7:
      return "tr/";
      break;
      */
    case 8:
      return "zh/";
    case 5:
      return "de/";
      break;
    default:
      return "local/";
    }  
  }

  std::string find_doc_prefix(int i){
    switch (i){
    case 1:
      return "doc/fr/";
      break;
    case 2:
      return "doc/en/";
      break;
    case 3:
      return "doc/es/";
      break;
    case 4:
      return "doc/el/";
      break;
    case 9:
      return "doc/pt/";
      break;
    case 6:
      return "doc/it/";
      break;
      /*
    case 7:
      return "doc/tr/";
      break;
      */
    case 8:
      return "doc/zh/";
      break;
    case 5:
      return "doc/de/";
      break;
    default:
      return "doc/local/";
    }  
  }

  void update_completions(){
    if (vector_completions_ptr()){
      vector_completions_ptr()->clear();
      int n=int(vector_aide_ptr()->size());
      for (int k=0;k<n;++k){
	if (debug_infolevel>10)
	  CERR << "+ " << (*vector_aide_ptr())[k].cmd_name  << '\n';
	vector_completions_ptr()->push_back((*vector_aide_ptr())[k].cmd_name);
      }
    }
  }

  void add_language(int i,GIAC_CONTEXT){
#ifdef FXCG
    return;
#else
    if (!equalposcomp(lexer_localization_vector(),i)){
      lexer_localization_vector().push_back(i);
      update_lexer_localization(lexer_localization_vector(),lexer_localization_map(),back_lexer_localization_map(),contextptr);
#if !defined(EMCC) && !defined(EMCC2)
      if (vector_aide_ptr()){
	// add locale command description
	int count;
	string filename=giac_aide_dir()+find_doc_prefix(i)+"aide_cas";
	readhelp(*vector_aide_ptr(),filename.c_str(),count,false);
	// add synonyms
	multimap<string,localized_string>::iterator it,backend=back_lexer_localization_map().end(),itend;
	vector<aide>::iterator jt = vector_aide_ptr()->begin(),jtend=vector_aide_ptr()->end();
	for (;jt!=jtend;++jt){
	  it=back_lexer_localization_map().find(jt->cmd_name);
	  itend=back_lexer_localization_map().upper_bound(jt->cmd_name);
	  if (it!=backend){
	    for (;it!=itend;++it){
	      if (it->second.language==i)
		jt->synonymes.push_back(it->second);
	    }
	  }
	}
	int s = int(vector_aide_ptr()->size());
	for (int j=0;j<s;++j){
	  aide a=(*vector_aide_ptr())[j];
	  it=back_lexer_localization_map().find(a.cmd_name);
	  itend=back_lexer_localization_map().upper_bound(a.cmd_name);
	  if (it!=backend){
	    for (;it!=itend;++it){
	      if (it->second.language==i){
		a.cmd_name=it->second.chaine;
		a.language=it->second.language;
		vector_aide_ptr()->push_back(a);
	      }
	    }
	  }
	}
#if !defined KHICAS && !defined SDL_KHICAS
	CERR << "Added " << vector_aide_ptr()->size()-s << " synonyms" << '\n';
#endif
	sort(vector_aide_ptr()->begin(),vector_aide_ptr()->end(),alpha_order);
	update_completions();
      }
#endif
    }
#endif // FXCG
  }

  void remove_language(int i,GIAC_CONTEXT){
#ifdef FXCG
    return;
#else
    if (int pos=equalposcomp(lexer_localization_vector(),i)){
      if (vector_aide_ptr()){
	vector<aide> nv;
	int s=int(vector_aide_ptr()->size());
	for (int j=0;j<s;++j){
	  if ((*vector_aide_ptr())[j].language!=i)
	    nv.push_back((*vector_aide_ptr())[j]);
	}
	*vector_aide_ptr() = nv;
	update_completions();
	vector<aide>::iterator jt = vector_aide_ptr()->begin(),jtend=vector_aide_ptr()->end();
	for (;jt!=jtend;++jt){
	  vector<localized_string> syno;
	  vector<localized_string>::const_iterator kt=jt->synonymes.begin(),ktend=jt->synonymes.end();
	  for (;kt!=ktend;++kt){
	    if (kt->language!=i)
	      syno.push_back(*kt);
	  }
	  jt->synonymes=syno;
	}
      }
      --pos;
      lexer_localization_vector().erase(lexer_localization_vector().begin()+pos);
      update_lexer_localization(lexer_localization_vector(), lexer_localization_map(), back_lexer_localization_map(), contextptr);
	}
#endif
  }

  int string2lang(const string & s){
    if (s=="fr")
      return 1;
    if (s=="en")
      return 2;
    if (s=="sp" || s=="es")
      return 3;
    if (s=="el")
      return 4;
    if (s=="pt")
      return 9;
    if (s=="it")
      return 6;
    if (s=="tr")
      return 7;
    if (s=="zh")
      return 8;
    if (s=="de")
      return 5;
    return 0;
  }

  std::string set_language(int i,GIAC_CONTEXT){
#if defined(EMCC) || defined(EMCC2)
    if (language(contextptr)!=i){
      language(i,contextptr);
      add_language(i,contextptr);
    }
#else
    language(i,contextptr);
    add_language(i,contextptr);
#endif
#if (defined KHICAS || defined SDL_KHICAS) && !defined NUMWORKS_SLOTBFR
    lang=i;
#endif
    return find_doc_prefix(i);
  }

  std::string read_env(GIAC_CONTEXT,bool verbose){
#ifndef RTOS_THREADX
#ifndef BESTA_OS
    if (getenv("GIAC_LAPACK")){
      CALL_LAPACK=atoi(getenv("GIAC_LAPACK"));
      if (verbose)
	CERR << "// Will call lapack if dimension is >=" << CALL_LAPACK << '\n';
    }
    if (getenv("GIAC_PADIC")){
      GIAC_PADIC=atoi(getenv("GIAC_PADIC"));
      if (verbose)
	CERR << "// Will use p-adic algorithm if dimension is >=" << GIAC_PADIC << '\n';
    }
#endif
#endif
    if (getenv("XCAS_RPN")){
      if (verbose)
	CERR << "// Setting RPN mode" << '\n';
      rpn_mode(contextptr)=true;
    }
    if (getenv("GIAC_XCAS_MODE")){
      xcas_mode(contextptr)=atoi(getenv("GIAC_XCAS_MODE"));
      if (verbose)
	CERR << "// Setting maple mode " << xcas_mode(contextptr) << '\n';
    }
    if (getenv("GIAC_C")){
      xcas_mode(contextptr)=0;
      if (verbose)
	CERR << "// Setting giac C mode" << '\n';
    }
    if (getenv("GIAC_MAPLE")){
      xcas_mode(contextptr)=1;
      if (verbose)
	CERR << "// Setting giac maple mode" << '\n';
    }
    if (getenv("GIAC_MUPAD")){
      xcas_mode(contextptr)=2;
      if (verbose)
	CERR << "// Setting giac mupad mode" << '\n';
    }
    if (getenv("GIAC_TI")){
      xcas_mode(contextptr)=3;
      if (verbose)
	CERR << "// Setting giac TI mode" << '\n';
    }
    if (getenv("GIAC_MONO")){
      if (verbose)
	CERR << "// Threads polynomial * disabled" << '\n';
      threads_allowed=false;
    }
    if (getenv("GIAC_MPZCLASS")){
      if (verbose)
	CERR << "// mpz_class enabled" << '\n';
      mpzclass_allowed=true;
    }
    if (getenv("GIAC_DEBUG")){
      debug_infolevel=atoi(getenv("GIAC_DEBUG"));
      CERR << "// Setting debug_infolevel to " << debug_infolevel << '\n';
    }
    if (getenv("GBASIS_COEFF_STRATEGY")){
      GBASIS_COEFF_STRATEGY=atoi(getenv("GBASIS_COEFF_STRATEGY"));
      CERR << "// Setting gbasis_coeff_strategy to " << GBASIS_COEFF_STRATEGY << '\n';
    }
    if (getenv("GBASIS_COEFF_MAXLOGRATIO")){
      GBASIS_COEFF_MAXLOGRATIO=atof(getenv("GBASIS_COEFF_MAXLOGRATIO"));
      CERR << "// Setting gbasis_coeff_maxlogratio to " << GBASIS_COEFF_MAXLOGRATIO << '\n';
    }
    if (getenv("GIAC_PRINTPROG")){ 
      // force print of prog at parse, 256 for python compat mode print
      printprog=atoi(getenv("GIAC_PRINTPROG"));
      CERR << "// Setting printprog to " << printprog << '\n';
    }
    string s;
    if (getenv("LANG"))
      s=getenv("LANG");
    else { // __APPLE__ workaround
#if !defined VISUALC && !defined NSPIRE && !defined FXCG
      if (!strcmp(gettext("File"),"Fich")){
	setenv("LANG","fr_FR.UTF8",1);
	s="fr_FR.UTF8";
      }
      else {
	s="en_US.UTF8";
	setenv("LANG",s.c_str(),1);
      }
      if (!strcmp(gettext("File"),"Datei")){
	setenv("LANG","de_DE.UTF8",1);
	s="de_DE.UTF8";
      }
#endif
    }
    if (debug_infolevel)
      cout << "LANG " << s << "\n";
    if (s.size()>=2){
      s=s.substr(0,2);
      int i=string2lang(s);
      if (i){
	language(i,contextptr);
	return find_doc_prefix(i);
      }
    }
    language(0,contextptr);
    return find_doc_prefix(0);
  }

  string cas_setup_string(GIAC_CONTEXT){
    string s("cas_setup(");
    s += print_VECT(cas_setup(contextptr),_SEQ__VECT,contextptr);
    s += "),";
    s += "xcas_mode(";
    s += print_INT_(xcas_mode(contextptr)+python_compat(contextptr)*256);
    s += ")";
    return s;
  }

  string geo_setup_string(){
    return xyztrange(gnuplot_xmin,gnuplot_xmax,gnuplot_ymin,gnuplot_ymax,gnuplot_zmin,gnuplot_zmax,gnuplot_tmin,gnuplot_tmax,global_window_xmin,global_window_xmax,global_window_ymin,global_window_ymax,_show_axes_,class_minimum,class_size,
#ifdef WITH_GNUPLOT
		     gnuplot_hidden3d,gnuplot_pm3d
#else
		     1,1
#endif
		     ).print(context0);
  }

  string add_extension(const string & s,const string & ext,const string & def){
    if (s.empty())
      return def+"."+ext;
    int i=int(s.size());
    for (--i;i>0;--i){
      if (s[i]=='.')
	break;
    }
    if (i<=0)
      return s+"."+ext;
    return s.substr(0,i)+"."+ext;
  }

#ifdef HAVE_LIBPTHREAD
  pthread_mutex_t context_list_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

  vector<context *> & context_list(){
    static vector<context *> * ans=0;
    if (!ans) ans=new vector<context *>(1,(context *) 0);
    return *ans;
  }
  context::context() { 
    // CERR << "new context " << this << '\n';
    parent=0;
    tabptr=new sym_tab; 
    globalcontextptr=this; previous=0; globalptr=new global; 
    quoted_global_vars=new vecteur;
    rootofs=new vecteur;
    history_in_ptr=new vecteur;
    history_out_ptr=new vecteur;
    history_plot_ptr=new vecteur;
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&context_list_mutex);
#endif
    context_list().push_back(this);
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_unlock(&context_list_mutex);
#endif
  }

#ifndef RTOS_THREADX
#if !defined BESTA_OS && !defined NSPIRE && !defined FXCG && !defined(KHICAS) && !defined SDL_KHICAS
  std::map<std::string,context *> * context_names = new std::map<std::string,context *> ;

  context::context(const string & name) { 
    // CERR << "new context " << this << '\n';
    parent=0;
    tabptr=new sym_tab; 
    globalcontextptr=this; previous=0; globalptr=new global; 
    quoted_global_vars=new vecteur;
    rootofs=new vecteur;
    history_in_ptr=new vecteur;
    history_out_ptr=new vecteur;
    history_plot_ptr=new vecteur;
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&context_list_mutex);
#endif
    context_list().push_back(this);
    if (context_names)
      (*context_names)[name]=this;
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_unlock(&context_list_mutex);
#endif
  }
#endif
#endif

  context::context(const context & c) { 
    *this = c;
  }

  context * context::clone() const{
    context * ptr = new context;
    *ptr->globalptr = *globalptr;
    return ptr;
  }

  void clear_context(context * ptr){
    if (!ptr)
      return;
    ptr->parent=0;
    if (ptr->history_in_ptr)
      delete ptr->history_in_ptr;
    if (ptr->history_out_ptr)
      delete ptr->history_out_ptr;
    if (ptr->history_plot_ptr)
      delete ptr->history_plot_ptr;
    if (ptr->quoted_global_vars)
      delete ptr->quoted_global_vars;
    if (ptr->rootofs)
      delete ptr->rootofs;
    if (ptr->globalptr)
      delete ptr->globalptr;
    if (ptr->tabptr)
      delete ptr->tabptr;
    ptr->tabptr=new sym_tab; 
    ptr->globalcontextptr=ptr; ptr->previous=0; ptr->globalptr=new global; 
    ptr->quoted_global_vars=new vecteur;
    ptr->rootofs=new vecteur;
    ptr->history_in_ptr=new vecteur;
    ptr->history_out_ptr=new vecteur;
    ptr->history_plot_ptr=new vecteur;
    //init_context(ptr);
  }

  void init_context(context * ptr){
    if (!ptr){
      CERR << "init_context on null context" << '\n';
      return;
    }
     ptr->globalptr->_xcas_mode_=_xcas_mode_;
#ifdef GIAC_HAS_STO_38
     ptr->globalptr->_calc_mode_=-38;
#else
     ptr->globalptr->_calc_mode_=_calc_mode_;
#endif
     ptr->globalptr->_decimal_digits_=_decimal_digits_;
     ptr->globalptr->_minchar_for_quote_as_string_=_minchar_for_quote_as_string_;
     ptr->globalptr->_scientific_format_=_scientific_format_;
     ptr->globalptr->_integer_format_=_integer_format_;
     ptr->globalptr->_integer_mode_=_integer_mode_;
     ptr->globalptr->_latex_format_=_latex_format_;
#ifdef BCD
     ptr->globalptr->_bcd_decpoint_=_bcd_decpoint_;
     ptr->globalptr->_bcd_mantissa_=_bcd_mantissa_;
     ptr->globalptr->_bcd_flags_=_bcd_flags_;
     ptr->globalptr->_bcd_printdouble_=_bcd_printdouble_;
#endif
     ptr->globalptr->_expand_re_im_=_expand_re_im_;
     ptr->globalptr->_do_lnabs_=_do_lnabs_;
     ptr->globalptr->_eval_abs_=_eval_abs_;
     ptr->globalptr->_eval_equaltosto_=_eval_equaltosto_;
     ptr->globalptr->_complex_mode_=_complex_mode_;
     ptr->globalptr->_escape_real_=_escape_real_;
     ptr->globalptr->_try_parse_i_=_try_parse_i_;
     ptr->globalptr->_specialtexprint_double_=_specialtexprint_double_;
     ptr->globalptr->_atan_tan_no_floor_=_atan_tan_no_floor_;
     ptr->globalptr->_keep_acosh_asinh_=_keep_acosh_asinh_;
     ptr->globalptr->_keep_algext_=_keep_algext_;
     ptr->globalptr->_auto_assume_=_auto_assume_;
     ptr->globalptr->_parse_e_=_parse_e_;
     ptr->globalptr->_convert_rootof_=_convert_rootof_;
     ptr->globalptr->_python_compat_=_python_compat_;
     ptr->globalptr->_complex_variables_=_complex_variables_;
     ptr->globalptr->_increasing_power_=_increasing_power_;
     ptr->globalptr->_approx_mode_=_approx_mode_;
     ptr->globalptr->_series_variable_name_=_series_variable_name_;
     ptr->globalptr->_series_default_order_=_series_default_order_;
     ptr->globalptr->_autosimplify_=_autosimplify_();
     ptr->globalptr->_lastprog_name_=_lastprog_name_();
     ptr->globalptr->_angle_mode_=_angle_mode_;
     ptr->globalptr->_variables_are_files_=_variables_are_files_;
     ptr->globalptr->_bounded_function_no_=_bounded_function_no_;
     ptr->globalptr->_series_flags_=_series_flags_; // bit1= full simplify, bit2=1 for truncation
     ptr->globalptr->_step_infolevel_=_step_infolevel_; // bit1= full simplify, bit2=1 for truncation
     ptr->globalptr->_local_eval_=_local_eval_;
     ptr->globalptr->_default_color_=_default_color_;
     ptr->globalptr->_epsilon_=_epsilon_<=0?1e-12:_epsilon_;
     ptr->globalptr->_proba_epsilon_=_proba_epsilon_;
     ptr->globalptr->_withsqrt_=_withsqrt_;
     ptr->globalptr->_show_point_=_show_point_; // show 3-d point 
     ptr->globalptr->_io_graph_=_io_graph_; // show 2-d point in io
     ptr->globalptr->_show_axes_=_show_axes_;
     ptr->globalptr->_spread_Row_=_spread_Row_;
     ptr->globalptr->_spread_Col_=_spread_Col_;
     ptr->globalptr->_printcell_current_row_=_printcell_current_row_;
     ptr->globalptr->_printcell_current_col_=_printcell_current_col_;
     ptr->globalptr->_all_trig_sol_=_all_trig_sol_;
     ptr->globalptr->_lexer_close_parenthesis_=_lexer_close_parenthesis_;
     ptr->globalptr->_rpn_mode_=_rpn_mode_;
     ptr->globalptr->_ntl_on_=_ntl_on_;
     ptr->globalptr->_prog_eval_level_val =_prog_eval_level_val ;
     ptr->globalptr->_eval_level=_eval_level;
     ptr->globalptr->_rand_seed=_rand_seed;
     ptr->globalptr->_language_=_language_;
     ptr->globalptr->_last_evaled_argptr_=_last_evaled_argptr_;
     ptr->globalptr->_last_evaled_function_name_=_last_evaled_function_name_;
     ptr->globalptr->_currently_scanned_="";
     ptr->globalptr->_max_sum_sqrt_=_max_sum_sqrt_;      
     ptr->globalptr->_max_sum_add_=_max_sum_add_;   
     
  }

  context * clone_context(const context * contextptr) {
    context * ptr = new context;
    if (contextptr){
      *ptr->globalptr = *contextptr->globalptr;
      *ptr->tabptr = *contextptr->tabptr;
    }
    else {
      init_context(ptr);
    }
    return ptr;
  }

  context::~context(){
    // CERR << "delete context " << this << '\n';
    if (!previous){
      if (history_in_ptr)
	delete history_in_ptr;
      if (history_out_ptr)
	delete history_out_ptr;
      if (history_plot_ptr)
	delete history_plot_ptr;
      if (quoted_global_vars)
	delete quoted_global_vars;
      if (rootofs)
	delete rootofs;
      if (globalptr)
	delete globalptr;
      if (tabptr)
	delete tabptr;
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_lock(&context_list_mutex);
#endif
      int s=int(context_list().size());
      for (int i=s-1;i>0;--i){
	if (context_list()[i]==this){
	  context_list().erase(context_list().begin()+i);
	  break;
	}
      }
#ifndef RTOS_THREADX
#if !defined BESTA_OS && !defined NSPIRE && !defined FXCG && !defined(KHICAS) && !defined SDL_KHICAS
      if (context_names){
	map<string,context *>::iterator it=context_names->begin(),itend=context_names->end();
	for (;it!=itend;++it){
	  if (it->second==this){
	    context_names->erase(it);
	    break;
	  }
	}
      }
#endif
#endif
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&context_list_mutex);
#endif
    }
  }

#ifndef CLK_TCK
#define CLK_TCK 1
#endif

#ifndef HAVE_NO_SYS_TIMES_H
   double delta_tms(struct tms tmp1,struct tms tmp2){
#if defined(HAVE_SYSCONF) && !defined(EMCC) && !defined(EMCC2)
     return double( tmp2.tms_utime+tmp2.tms_stime+tmp2.tms_cutime+tmp2.tms_cstime-(tmp1.tms_utime+tmp1.tms_stime+tmp1.tms_cutime+tmp1.tms_cstime) )/sysconf(_SC_CLK_TCK);
#else
    return double( tmp2.tms_utime+tmp2.tms_stime+tmp2.tms_cutime+tmp2.tms_cstime-(tmp1.tms_utime+tmp1.tms_stime+tmp1.tms_cutime+tmp1.tms_cstime) )/CLK_TCK;
#endif
   }
#elif defined(__MINGW_H)
  double delta_tms(clock_t tmp1,clock_t tmp2) {
    return (double)(tmp2-tmp1)/CLOCKS_PER_SEC;
  }
#endif /// HAVE_NO_SYS_TIMES_H

  string remove_filename(const string & s){
    int l=int(s.size());
    for (;l;--l){
      if (s[l-1]=='/')
	break;
    }
    return s.substr(0,l);
  }

#ifdef HAVE_LIBPTHREAD
  static void * in_thread_eval(void * arg){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
    vecteur *v = (vecteur *) arg;
    context * contextptr=(context *) (*v)[2]._POINTER_val;
    thread_param * ptr =thread_param_ptr(contextptr);
    pthread_attr_getstacksize(&ptr->attr,&ptr->stacksize);
    ptr->stackaddr=(void *) ((uintptr_t) &ptr-ptr->stacksize);
    ptr->stack=(size_t) &ptr;
#ifndef __MINGW_H
    struct tms tmp1,tmp2;
    times(&tmp1);
#else
    int beg=CLOCK();
#endif
    gen g = (*v)[0];
    g = protecteval(g,(*v)[1].val,contextptr);
#ifndef NO_STDEXCEPT
    try {
#endif
#ifndef __MINGW_H
      times(&tmp2);
      double dt=delta_tms(tmp1,tmp2);
      total_time(contextptr) += dt;
      (*v)[4]=dt;
#else
      int end=CLOCK();
      (*v)[4]=end-beg;
#endif
      (*v)[5]=g;
#ifndef NO_STDEXCEPT
    } catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
    }
#endif
    ptr->stackaddr=0; ptr->stack=0;
    thread_eval_status(0,contextptr);
    pthread_exit(0);
    return 0;
  }

  // create a new thread for evaluation of g at level level in context
  bool make_thread(const gen & g,int level,const giac_callback & f,void * f_param,const context * contextptr){
    if (is_context_busy(contextptr))
      return false;
    thread_param * ptr =thread_param_ptr(contextptr);
    if (!ptr || ptr->v.size()!=6)
      return false;
    pthread_mutex_lock(mutexptr(contextptr));
    ptr->v[0]=g;
    ptr->v[1]=level;
    ptr->v[2]=gen((void *)contextptr,_CONTEXT_POINTER);
    ptr->f=f;
    ptr->f_param=f_param;
    thread_eval_status(1,contextptr);
    pthread_attr_init(&ptr->attr);
    int cres=pthread_create (&ptr->eval_thread, &ptr->attr, in_thread_eval,(void *)&ptr->v);
    if (cres){
      thread_eval_status(0,contextptr);
      pthread_mutex_unlock(mutexptr(contextptr));
    }
    return !cres;
  }

  // check if contextptr has a running evaluation thread
  // if not returns -1
  // if evaluation is not finished return 1
  // if evaluation is finished, clear mutex lock and 
  // call the thread_param_ptr callback function with the evaluation value
  // and returns 0
  // otherwise returns status, 2=debug, 3=wait click
  int check_thread(context * contextptr){
    if (!is_context_busy(contextptr))
      return -1;
    int status=thread_eval_status(contextptr);
    if (status!=0 && !kill_thread(contextptr))
      return status;
    thread_param tp = *thread_param_ptr(contextptr);
    if (status==0){
      // unsigned thread_return_value=0;
      // void * ptr_return=&thread_return_value;
      // pthread_join(eval_thread,&ptr_return);
      if (
#ifdef __MINGW_H
	  1
#else
	  tp.eval_thread
#endif
	  ){
	giac_callback f=tp.f;
	gen arg_callback=tp.v[5];
	void * param_callback=tp.f_param;
	double tt=tp.v[4]._DOUBLE_val;
	pthread_join(tp.eval_thread,0); 
	pthread_mutex_unlock(mutexptr(contextptr));
	// double tt=double(tp.v[4].val)/CLOCKS_PER_SEC;
	if (tt>0.4)
	  (*logptr(contextptr)) << gettext("\nEvaluation time: ") << tt << '\n';
	if (f)
	  f(arg_callback,param_callback);
	else
	  (*logptr(contextptr)) << arg_callback << '\n';
	return 0;
      }
    }
    if (kill_thread(contextptr)==1){
      kill_thread(0,contextptr);
      thread_eval_status(0,contextptr);
      clear_prog_status(contextptr);
      cleanup_context(contextptr);
      if (tp.f)
	tp.f(string2gen("Aborted",false),tp.f_param);
#if !defined __MINGW_H && !defined KHICAS && !defined SDL_KHICAS
      *logptr(contextptr) << gettext("Thread ") << tp.eval_thread << " has been cancelled" << '\n';
#endif
#ifdef NO_STDEXCEPT
      pthread_cancel(tp.eval_thread) ;
#else
      try {
	pthread_cancel(tp.eval_thread) ;
      } catch (...){
      }
#endif
      pthread_mutex_unlock(mutexptr(contextptr));
      return -1;
    }
    return status;
  }

  // check contexts in context_list starting at index i, 
  // returns at first context with status >= 2
  // return value is -2 (invalid range), -1 (ok) or context number
  int check_threads(int i){
    int s,ans=-1;
    context * cptr;
    if (// i>=s || 
	i<0)
      return -2;
    for (;;++i){
      pthread_mutex_lock(&context_list_mutex);
      s=context_list().size();
      if (i<s)
	cptr=context_list()[i];
      pthread_mutex_unlock(&context_list_mutex);
      if (i>=s)
	break;
      int res=check_thread(cptr);
      if (res>1){
	ans=i;
	break;
      }
    }
    return ans;
  }

  gen thread_eval(const gen & g_,int level,context * contextptr,void (* wait_0001)(context *) ){
    gen g=equaltosto(g_,contextptr);
    /* launch a new thread for evaluation only,
       no more readqueue, readqueue is done by the "parent" thread
       Ctrl-C will kill the "child" thread
       wait_001 is a function that should wait 0.001 s and update thinks
       for example it could remove idle callback of a GUI
       then call the wait function of the GUI and readd callbacks
    */
    pthread_t eval_thread;
    vecteur v(6);
    v[0]=g;
    v[1]=level;
    v[2]=gen(contextptr,_CONTEXT_POINTER);
    pthread_mutex_lock(mutexptr(contextptr));
    thread_eval_status(1,contextptr);
    int cres=pthread_create (&eval_thread, (pthread_attr_t *) NULL, in_thread_eval,(void *)&v);
    if (!cres){
      for (;;){
	int eval_status=thread_eval_status(contextptr);
	if (!eval_status)
	  break;
	wait_0001(contextptr);
	if (kill_thread(contextptr)==1){
	  kill_thread(0,contextptr);
	  clear_prog_status(contextptr);
	  cleanup_context(contextptr);
#if !defined __MINGW_H && !defined KHICAS && !defined SDL_KHICAS
	  *logptr(contextptr) << gettext("Cancel thread ") << eval_thread << '\n';
#endif
#ifdef NO_STDEXCEPT
	  pthread_cancel(eval_thread) ;
#else
	  try {
	    pthread_cancel(eval_thread) ;
	  } catch (...){
	  }
#endif
	  pthread_mutex_unlock(mutexptr(contextptr));
	  return undef;
	}
      }
      // unsigned thread_return_value=0;
      // void * ptr=&thread_return_value;
      pthread_join(eval_thread,0); // pthread_join(eval_thread,&ptr);
      // Restore pointers and return v[3]
      pthread_mutex_unlock(mutexptr(contextptr));
      // double tt=double(v[4].val)/CLOCKS_PER_SEC;
      double tt=v[4]._DOUBLE_val;
      if (tt>0.1)
	(*logptr(contextptr)) << gettext("Evaluation time: ") << tt << '\n';
      return v[5];
    }
    pthread_mutex_unlock(mutexptr(contextptr));
    return protecteval(g,level,contextptr);
  }
#else

  bool make_thread(const gen & g,int level,const giac_callback & f,void * f_param,const context * contextptr){
    return false;
  }

  int check_thread(context * contextptr){
    return -1;
  }

  int check_threads(int i){
    return -1;
  }

  gen thread_eval(const gen & g,int level,context * contextptr,void (* wait_001)(context * )){
    return protecteval(g,level,contextptr);
  }
#endif // HAVE_LIBPTHREAD

  debug_struct::debug_struct():indent_spaces(0),debug_mode(false),sst_mode(false),sst_in_mode(false),debug_allowed(true),current_instruction(-1),debug_refresh(false){
    debug_info_ptr=new gen;
    fast_debug_info_ptr=new gen;
    debug_prog_name=new gen;
    debug_localvars=new gen;
    debug_contextptr=0;
  }
  
  debug_struct::~debug_struct(){
    delete debug_info_ptr;
    delete fast_debug_info_ptr;
    delete debug_prog_name;
    delete debug_localvars;
  }

  debug_struct & debug_struct::operator =(const debug_struct & dbg){
    indent_spaces=dbg.indent_spaces;
    args_stack=dbg.args_stack;
    debug_breakpoint=dbg.debug_breakpoint;
    debug_watch=dbg.debug_watch ;
    debug_mode=dbg.debug_mode;
    sst_mode=dbg.sst_mode ;
    sst_in_mode=dbg.sst_in_mode ;
    debug_allowed=dbg.debug_allowed;
    current_instruction_stack=dbg.current_instruction_stack;
    current_instruction=dbg.current_instruction;
    sst_at_stack=dbg.sst_at_stack;
    sst_at=dbg.sst_at;
    if (debug_info_ptr)
      delete debug_info_ptr;
    debug_info_ptr=new gen(dbg.debug_info_ptr?*dbg.debug_info_ptr:0) ;
    if (fast_debug_info_ptr)
      delete fast_debug_info_ptr;
    fast_debug_info_ptr= new gen(dbg.fast_debug_info_ptr?*dbg.fast_debug_info_ptr:0);
    if (debug_prog_name)
      delete debug_prog_name;
    debug_prog_name=new gen(dbg.debug_prog_name?*dbg.debug_prog_name:0);
    if (debug_localvars)
      delete debug_localvars;
    debug_localvars=new gen(dbg.debug_localvars?*dbg.debug_localvars:0);
    debug_refresh=dbg.debug_refresh;
    debug_contextptr=dbg.debug_contextptr;
    return *this;
  }

  static debug_struct & _debug_data(){
    static debug_struct * ans = 0;
    if (!ans) ans=new debug_struct;
    return *ans;
  }

  debug_struct * debug_ptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      return contextptr->globalptr->_debug_ptr;
    return &_debug_data();
  }

  void clear_prog_status(GIAC_CONTEXT){
    debug_struct * ptr=debug_ptr(contextptr);
    if (ptr){
      ptr->args_stack.clear();
      ptr->debug_mode=false;
      ptr->sst_at_stack.clear();
      if (!contextptr)
	protection_level=0;
    }
  }


  global::global() : _xcas_mode_(0), 
		     _calc_mode_(0),_decimal_digits_(12),_minchar_for_quote_as_string_(1),
		     _scientific_format_(0), _integer_format_(0), _latex_format_(0), 
#ifdef BCD
		     _bcd_decpoint_('.'|('E'<<16)|(' '<<24)),_bcd_mantissa_(12+(15<<8)), _bcd_flags_(0),_bcd_printdouble_(false),
#endif
		     _expand_re_im_(true), _do_lnabs_(true), _eval_abs_(true),_eval_equaltosto_(true),_integer_mode_(true),_complex_mode_(false), _escape_real_(true),_complex_variables_(false), _increasing_power_(false), _approx_mode_(false), _variables_are_files_(false), _local_eval_(true), 
		     _withsqrt_(true), 
		     _show_point_(true),  _io_graph_(true),
		     _all_trig_sol_(false),
#ifdef __MINGW_H
		     _ntl_on_(false),
#else
		     _ntl_on_(true),
#endif
#ifdef WITH_MYOSTREAM
		     _lexer_close_parenthesis_(true),_rpn_mode_(false),_try_parse_i_(true),_specialtexprint_double_(false),_atan_tan_no_floor_(false),_keep_acosh_asinh_(false),_keep_algext_(false),_auto_assume_(false),_parse_e_(false),_convert_rootof_(true),
#ifdef KHICAS
		     _python_compat_(true),
#else
		     _python_compat_(false),
#endif
		     _angle_mode_(0), _bounded_function_no_(0), _series_flags_(0x3),_step_infolevel_(0),_default_color_(FL_BLACK), _epsilon_(1e-12), _proba_epsilon_(1e-15),  _show_axes_(1),_spread_Row_ (-1), _spread_Col_ (-1),_logptr_(&my_CERR),_prog_eval_level_val(1), _eval_level(DEFAULT_EVAL_LEVEL), _rand_seed(123457),_last_evaled_function_name_(0),_currently_scanned_(""),_last_evaled_argptr_(0),_max_sum_sqrt_(3),
#ifdef GIAC_HAS_STO_38 // Prime sum(x^2,x,0,100000) crash on hardware	
		     _max_sum_add_(10000),
#else
		     _max_sum_add_(100000),
#endif
		     _total_time_(0),_evaled_table_(0),_extra_ptr_(0),_series_variable_name_('h'),_series_default_order_(5),
#else
		     _lexer_close_parenthesis_(true),_rpn_mode_(false),_try_parse_i_(true),_specialtexprint_double_(false),_atan_tan_no_floor_(false),_keep_acosh_asinh_(false),_keep_algext_(false),_auto_assume_(false),_parse_e_(false),_convert_rootof_(true),
#ifdef KHICAS
		     _python_compat_(true),
#else
		     _python_compat_(false),
#endif
		     _angle_mode_(0), _bounded_function_no_(0), _series_flags_(0x3),_step_infolevel_(0),_default_color_(FL_BLACK), _epsilon_(1e-12), _proba_epsilon_(1e-15),  _show_axes_(1),_spread_Row_ (-1), _spread_Col_ (-1), 
#if (defined(EMCC) || defined(EMCC2)) && !defined SDL_KHICAS
		     _logptr_(&COUT), 
#else
#ifdef FXCG
		     _logptr_(0),
#else
#if defined KHICAS || defined SDL_KHICAS
		     _logptr_(&os_cerr),
#else
		     _logptr_(&CERR),
#endif
#endif
#endif
		     _prog_eval_level_val(1), _eval_level(DEFAULT_EVAL_LEVEL), _rand_seed(123457),_last_evaled_function_name_(0),_currently_scanned_(""),_last_evaled_argptr_(0),_max_sum_sqrt_(3),
#ifdef GIAC_HAS_STO_38 // Prime sum(x^2,x,0,100000) crash on hardware	
		     _max_sum_add_(10000),
#else
		     _max_sum_add_(100000),
#endif
		     _total_time_(0),_evaled_table_(0),_extra_ptr_(0),_series_variable_name_('h'),_series_default_order_(5)
#endif
  { 
    _pl._i_sqrt_minus1_=1;
#if !defined KHICAS && !defined SDL_KHICAS
    _turtle_stack_.push_back(_turtle_);
#endif
    _debug_ptr=new debug_struct;
    _thread_param_ptr=new thread_param;
    _parsed_genptr_=new gen;
#ifdef GIAC_HAS_STO_38
    _autoname_="GA";
#else
    _autoname_="A";
#endif
    _autosimplify_="regroup";
    _lastprog_name_="lastprog";
    _format_double_="";
#ifdef HAVE_LIBPTHREAD
    _mutexptr = new pthread_mutex_t;
    pthread_mutex_init(_mutexptr,0);
    _mutex_eval_status_ptr = new pthread_mutex_t;
    pthread_mutex_init(_mutex_eval_status_ptr,0);
#endif
  }

  global & global::operator = (const global & g){
     _xcas_mode_=g._xcas_mode_;
     _calc_mode_=g._calc_mode_;
     _decimal_digits_=g._decimal_digits_;
     _minchar_for_quote_as_string_=g._minchar_for_quote_as_string_;
     _scientific_format_=g._scientific_format_;
     _integer_format_=g._integer_format_;
     _integer_mode_=g._integer_mode_;
     _latex_format_=g._latex_format_;
#ifdef BCD
     _bcd_decpoint_=g._bcd_decpoint_;
     _bcd_mantissa_=g._bcd_mantissa_;
     _bcd_flags_=g._bcd_flags_;
     _bcd_printdouble_=g._bcd_printdouble_;
#endif
     _expand_re_im_=g._expand_re_im_;
     _do_lnabs_=g._do_lnabs_;
     _eval_abs_=g._eval_abs_;
     _eval_equaltosto_=g._eval_equaltosto_;
     _complex_mode_=g._complex_mode_;
     _escape_real_=g._escape_real_;
     _complex_variables_=g._complex_variables_;
     _increasing_power_=g._increasing_power_;
     _approx_mode_=g._approx_mode_;
     _series_variable_name_=g._series_variable_name_;
     _series_default_order_=g._series_default_order_;
     _angle_mode_=g._angle_mode_;
     _atan_tan_no_floor_=g._atan_tan_no_floor_;
     _keep_acosh_asinh_=g._keep_acosh_asinh_;
     _keep_algext_=g._keep_algext_;
     _auto_assume_=g._auto_assume_;
     _parse_e_=g._parse_e_;
     _convert_rootof_=g._convert_rootof_;
     _python_compat_=g._python_compat_;
     _variables_are_files_=g._variables_are_files_;
     _bounded_function_no_=g._bounded_function_no_;
     _series_flags_=g._series_flags_; // bit1= full simplify, bit2=1 for truncation, bit3=?, bit4=1 do not convert back SPOL1 to symbolic expression
     _step_infolevel_=g._step_infolevel_; // bit1= full simplify, bit2=1 for truncation
     _local_eval_=g._local_eval_;
     _default_color_=g._default_color_;
     _epsilon_=g._epsilon_;
     _proba_epsilon_=g._proba_epsilon_;
     _withsqrt_=g._withsqrt_;
     _show_point_=g._show_point_; // show 3-d point 
     _io_graph_=g._io_graph_; // show 2-d point in io
     _show_axes_=g._show_axes_;
     _spread_Row_=g._spread_Row_;
     _spread_Col_=g._spread_Col_;
     _printcell_current_row_=g._printcell_current_row_;
     _printcell_current_col_=g._printcell_current_col_;
     _all_trig_sol_=g._all_trig_sol_;
     _ntl_on_=g._ntl_on_;
     _prog_eval_level_val =g._prog_eval_level_val ;
     _eval_level=g._eval_level;
     _rand_seed=g._rand_seed;
     _language_=g._language_;
     _last_evaled_argptr_=g._last_evaled_argptr_;
     _last_evaled_function_name_=g._last_evaled_function_name_;
     _currently_scanned_=g._currently_scanned_;
     _max_sum_sqrt_=g._max_sum_sqrt_;
     _max_sum_add_=g._max_sum_add_;
     _turtle_=g._turtle_;
#if !defined KHICAS && !defined SDL_KHICAS
     _turtle_stack_=g._turtle_stack_;
#endif
     _autoname_=g._autoname_;
     _format_double_=g._format_double_;
     _extra_ptr_=g._extra_ptr_;
     return *this;
  }

  global::~global(){
    delete _parsed_genptr_;
    delete _thread_param_ptr;
    delete _debug_ptr;
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_destroy(_mutexptr);
    delete _mutexptr;
    pthread_mutex_destroy(_mutex_eval_status_ptr);
    delete _mutex_eval_status_ptr;
#endif
  }

#ifdef FXCG
  bool my_isinf(double d){
    return 1/d==0.0;
  }
  bool my_isnan(double d){
    return d==d+1 && !my_isinf(d);
  }
#else // FXCG
#ifdef __APPLE__
  bool my_isnan(double d){
#if 1 // TARGET_OS_IPHONE
    return isnan(d);
#else
    return __isnand(d);
#endif
  }

  bool my_isinf(double d){
#if 1  // TARGET_OS_IPHONE
    return isinf(d);
#else
    return __isinfd(d);
#endif
  }

#else // __APPLE__
  bool my_isnan(double d){
#if defined VISUALC || defined BESTA_OS
#if !defined RTOS_THREADX && !defined FREERTOS
    return _isnan(d)!=0;
#else
    return isnan(d);
#endif
#else
#if defined(FIR_LINUX) || defined(FIR_ANDROID)
    return std::isnan(d);
#else
    return isnan(d);
#endif
#endif
  }

  bool my_isinf(double d){
#if defined VISUALC || defined BESTA_OS
    double x=0.0;
    return d==1.0/x || d==-1.0/x;
#else
#if defined(FIR_LINUX) || defined(FIR_ANDROID)
    return std::isinf(d);
#else
    return isinf(d);
#endif
#endif
  }

#endif // __APPLE__
#endif // FXCG
  
  double giac_floor(double d){
    double maxdouble=longlong(1)<<30;
    if (d>=maxdouble || d<=-maxdouble)
      return std::floor(d);
    if (d>0)
      return int(d);
    double k=int(d);
    if (k==d)
      return k;
    else
      return k-1;
  }
  double giac_ceil(double d){
    double maxdouble=longlong(1)<<54;
    if (d>=maxdouble || d<=-maxdouble)
      return d;
    if (d<0)
      return double(longlong(d));
    double k=double(longlong(d));
    if (k==d)
      return k;
    else
      return k+1;
  }



/* --------------------------------------------------------------------- */
/*
 * Copyright 2001-2004 Unicode, Inc.
 * 
 * Disclaimer
 * 
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 * 
 * Limitations on Rights to Redistribute This Code
 * 
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

    Conversions between UTF-16 and UTF-8. Source code file.
    Author: Mark E. Davis, 1994.
    Rev History: Rick McGowan, fixes & updates May 2001.
    Sept 2001: fixed const & error conditions per
    mods suggested by S. Parent & A. Lillich.
    June 2002: Tim Dodd added detection and handling of incomplete
    source sequences, enhanced error detection, added casts
    to eliminate compiler warnings.
    July 2003: slight mods to back out aggressive FFFE detection.
    Jan 2004: updated switches in from-UTF8 conversions.
    Oct 2004: updated to use UNI_MAX_LEGAL_UTF32 in UTF-32 conversions.
    Jan 2013: Jean-Yves Avenard adapted to only calculate size if 
    destination pointer are null

------------------------------------------------------------------------ */


static const int halfShift  = 10; /* used for shifting by 10 bits */

static const UTF32 halfBase = 0x0010000UL;
static const UTF32 halfMask = 0x3FFUL;

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

/* --------------------------------------------------------------------- */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const char trailingBytesForUTF8[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const UTF32 offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL, 
             0x03C82080UL, 0xFA082080UL, 0x82082080UL };

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const UTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/* --------------------------------------------------------------------- */

/* The interface converts a whole buffer to avoid function-call overhead.
 * Constants have been gathered. Loops & conditionals have been removed as
 * much as possible for efficiency, in favor of drop-through switches.
 * (See "Note A" at the bottom of the file for equivalent code.)
 * If your compiler supports it, the "isLegalUTF8" call can be turned
 * into an inline function.
 */

/* --------------------------------------------------------------------- */

unsigned int ConvertUTF16toUTF8 (
    const UTF16* sourceStart, const UTF16* sourceEnd, 
    UTF8* targetStart, UTF8* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF16* source = sourceStart;
    UTF8* target = targetStart;
    UTF32 ch;
    while ((source < sourceEnd) && (ch = *source)) {
    unsigned short bytesToWrite = 0;
    const UTF32 byteMask = 0xBF;
    const UTF32 byteMark = 0x80; 
    const UTF16* oldSource = source; /* In case we have to back up because of target overflow. */
    source++;
    /* If we have a surrogate pair, convert to UTF32 first. */
    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
        /* If the 16 bits following the high surrogate are in the source buffer... */
        UTF32 ch2;
        if ((source < sourceEnd) && (ch2 = *source)) {
        /* If it's a low surrogate, convert to UTF32. */
        if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
            ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
            + (ch2 - UNI_SUR_LOW_START) + halfBase;
            ++source;
        } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
            --source; /* return to the illegal value itself */
            result = sourceIllegal;
            break;
        }
        } else { /* We don't have the 16 bits following the high surrogate. */
        --source; /* return to the high surrogate */
        result = sourceExhausted;
        break;
        }
    } else if (flags == strictConversion) {
        /* UTF-16 surrogate values are illegal in UTF-32 */
        if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
        --source; /* return to the illegal value itself */
        result = sourceIllegal;
        break;
        }
    }
    /* Figure out how many bytes the result will require */
    if (ch < (UTF32)0x80) {      bytesToWrite = 1;
    } else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
    } else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
    } else if (ch < (UTF32)0x110000) {  bytesToWrite = 4;
    } else {       bytesToWrite = 3;
                        ch = UNI_REPLACEMENT_CHAR;
    }

    target += bytesToWrite;
    if ((uintptr_t)target > (uintptr_t)targetEnd) {
        source = oldSource; /* Back up source pointer! */
        target -= bytesToWrite; result = targetExhausted; break;
    }
    switch (bytesToWrite) { /* note: everything falls through. */
        case 4: target--; if (targetStart) { *target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6; }
        case 3: target--; if (targetStart) { *target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6; }
        case 2: target--; if (targetStart) { *target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6; }
        case 1: target--; if (targetStart) { *target =  (UTF8)(ch | firstByteMark[bytesToWrite]); }
    }
    target += bytesToWrite;
    }

    unsigned int length = int(target - targetStart);
    return length;
}

/* --------------------------------------------------------------------- */

/*
 * Utility routine to tell whether a sequence of bytes is legal UTF-8.
 * This must be called with the length pre-determined by the first byte.
 * If not calling this from ConvertUTF8to*, then the length can be set by:
 *  length = trailingBytesForUTF8[*source]+1;
 * and the sequence is illegal right away if there aren't that many bytes
 * available.
 * If presented with a length > 4, this returns false.  The Unicode
 * definition of UTF-8 goes up to 4-byte sequences.
 */

static Boolean isLegalUTF8(const UTF8 *source, int length) {
    UTF8 a;
    const UTF8 *srcptr = source+length;
    switch (length) {
    default: return false;
    /* Everything else falls through when "true"... */
    case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    case 2: if ((a = (*--srcptr)) > 0xBF) return false;

    switch (*source) {
        /* no fall-through in this inner switch */
        case 0xE0: if (a < 0xA0) return false; break;
        case 0xED: if (a > 0x9F) return false; break;
        case 0xF0: if (a < 0x90) return false; break;
        case 0xF4: if (a > 0x8F) return false; break;
        default:   if (a < 0x80) return false;
    }

    case 1: if (*source >= 0x80 && *source < 0xC2) return false;
    }
    if (*source > 0xF4) return false;
    return true;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 sequence is legal or not.
 * This is not used here; it's just exported.
 */
Boolean isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd) {
    int length = trailingBytesForUTF8[*source]+1;
    if (source+length > sourceEnd) {
    return false;
    }
    return isLegalUTF8(source, length);
}

/* --------------------------------------------------------------------- */

unsigned int ConvertUTF8toUTF16 (const UTF8* sourceStart, const UTF8* sourceEnd, UTF16* targetStart, UTF16* targetEnd, ConversionFlags flags)
#if 0 //def GIAC_HAS_STO_38
{
    wchar_t *d= targetStart;
#define read(a) if (sourceStart>=sourceEnd) break; a= *sourceStart++
    while (sourceStart<sourceEnd && d!=targetEnd)
    {
        read(uint8_t c);
        if ((c&0x80)==0) { *d++= c; continue; }
        if ((c&0xe0)==0xc0) { read(uint8_t c2); if ((c2&0xc0)!=0x80) continue; *d++= ((c&0x1f)<<6)+(c2&0x3f); continue; }
        if ((c&0xf0)==0xe0) { read(uint8_t c2); if ((c2&0xc0)!=0x80) continue; read(uint8_t c3); if ((c3&0xc0)!=0x80) continue; *d++= ((((c&0xf)<<6)+(c2&0x3f))<<6)+(c3&0x3f); continue; }
        if ((c&0xf8)==0xf0) { read(uint8_t c2); if ((c2&0xc0)!=0x80) continue; read(uint8_t c3); if ((c3&0xc0)!=0x80) continue; read(uint8_t c4); if ((c4&0xc0)!=0x80) continue; *d++= ((((((c&0xf)<<6)+(c2&0x3f))<<6)+(c3&0x3f))<<6)+(c4&0x3f); continue; }
        while (sourceStart<sourceEnd) { c= *sourceEnd; if ((c&0x80)==0 || (c&0xc0)!=0x80) break; sourceEnd++; }
    }
#undef read
    return d-targetStart;
}

unsigned int ConvertUTF8toUTF162 (
    const UTF8* sourceStart, const UTF8* sourceEnd, 
    UTF16* targetStart, UTF16* targetEnd, ConversionFlags flags) 
#endif
{
    ConversionResult result = conversionOK;
    const UTF8* source = sourceStart;
    UTF16* target = targetStart;
    while (source < sourceEnd && *source) {
    UTF32 ch = 0;
    unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
    if (source + extraBytesToRead >= sourceEnd) {
        result = sourceExhausted; break;
    }
    /* Do this check whether lenient or strict */
    if (! isLegalUTF8(source, extraBytesToRead+1)) {
        result = sourceIllegal;
        break;
    }
    /*
     * The cases all fall through. See "Note A" below.
     */
    switch (extraBytesToRead) {
        case 5: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
        case 4: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
        case 3: ch += *source++; ch <<= 6;
        case 2: ch += *source++; ch <<= 6;
        case 1: ch += *source++; ch <<= 6;
        case 0: ch += *source++;
    }
    ch -= offsetsFromUTF8[extraBytesToRead];

    if ((uintptr_t)target >= (uintptr_t)targetEnd) {
        source -= (extraBytesToRead+1); /* Back up source pointer! */
        result = targetExhausted; break;
    }
    if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
        /* UTF-16 surrogate values are illegal in UTF-32 */
        if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
        if (flags == strictConversion) {
            source -= (extraBytesToRead+1); /* return to the illegal value itself */
            result = sourceIllegal;
            break;
        } else {
          if (targetStart)
            *target = UNI_REPLACEMENT_CHAR;
          target++;
        }
        } else {
          if (targetStart)
            *target = (UTF16)ch; /* normal case */
          target++;
        }
    } else if (ch > UNI_MAX_UTF16) {
        if (flags == strictConversion) {
        result = sourceIllegal;
        source -= (extraBytesToRead+1); /* return to the start */
        break; /* Bail out; shouldn't continue */
        } else {
        *target++ = UNI_REPLACEMENT_CHAR;
        }
    } else {
        /* target is a character in range 0xFFFF - 0x10FFFF. */
        if ((uintptr_t)target + 1 >= (uintptr_t)targetEnd) {
        source -= (extraBytesToRead+1); /* Back up source pointer! */
        result = targetExhausted; break;
        }
        ch -= halfBase;
        if (targetStart)
        {
          *target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
          *target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
        }
        else
          target += 2;
    }
    }

    unsigned int length = unsigned(target - targetStart);
    return length;
}

  unsigned int utf82unicode(const char * line, wchar_t * wline, unsigned int n){
    if (!line){
      if (wline) wline[0]=0;
      return 0;
    }

    unsigned int j = ConvertUTF8toUTF16 (
      (const UTF8*) line,((line + n) < line) ? (const UTF8*)~0 : (const UTF8*)(line + n),
      (UTF16*)wline, (UTF16*)~0,
      lenientConversion);

    if (wline) wline[j] = 0;

    return j;
  }

    // convert position n in utf8-encoded line into the corresponding position 
  // in the same string encoded with unicode 
  unsigned int utf8pos2unicodepos(const char * line,unsigned int n,bool skip_added_spaces){
    if (!line) return 0;
    unsigned int i=0,j=0,c;
    for (;i<n;i++){
      c=line[i];
      if (!c)
	return j;
      if ( (c & 0xc0) == 0x80)
	continue;
      if (c < 128){ // 0/xxxxxxx/
	j++;
	continue;
      }
      if ( (c & 0xe0) == 0xc0) { // 2 char code 110/xxxxx/ 10/xxxxxx/
	i++;
	c = (c & 0x1f) << 6 | (line[i] & 0x3f);
	j++;
	continue;
      } 
      if ( (c & 0xf0) == 0xe0) { // 3 char 1110/xxxx/ 10/xxxxxx/ 10/xxxxxx/
	i++;
	c = (c & 0x0f) << 6 | (line[i] & 0x3f);
	i++;
	c = c << 6 | (line[i] & 0x3f);
	j++;
	if (skip_added_spaces) {
	  unsigned int masked = c & 0xff00; // take care of spaces that were added
	  if (masked>=0x2000 && masked<0x2c00)
	    j -= 2;
	}
	continue;
      } 
      if ( (c & 0xf8) == 0xf0) { // 4 char 11110/xxx/ 10/xxxxxx/ 10/xxxxxx/ 10/xxxxxx/
	i++;
	c = (c & 0x07) << 6 | (line[i] & 0x3f);
	i++;
	c = c << 6 | (line[i] & 0x3f);
	i++;
	c = c << 6 | (line[i] & 0x3f);
	j++;
	continue;
      } 
      // FIXME complete for 5 and 6 char
      c = 0xfffd;
      j++;
    }
    return j;
  }

  unsigned int wstrlen(const char * line, unsigned int n){
    if (!line) return 0;
    return utf82unicode(line, NULL, n);
  }

  // convert UTF8 string to unicode, allocate memory with new
  wchar_t * utf82unicode(const char * idname){
    if (!idname)
      return 0;
    int l=int(strlen(idname));
    wchar_t * wname=new wchar_t[l+1];
    utf82unicode(idname,wname,l);
    return wname;
  }

#if defined NSPIRE || defined FXCG
  unsigned wcslen(const wchar_t * c){
    unsigned i=0;
    for (;*c;++i)
      ++c;
    return i;
  }
#endif

  char * unicode2utf8(const wchar_t * idname){
    if (!idname)
      return 0;
    int l=int(wcslen(idname));
    char * name=new char[4*l+1];
    unicode2utf8(idname,name,l);
    return name;
  }

  unsigned int wstrlen(const wchar_t * wline){
    if (!wline)
      return 0;
    unsigned int i=0;
    for (;*wline;wline++){ i++; }
    return i;
  }

  // return length required to translate from unicode to UTF8
  unsigned int utf8length(const wchar_t * wline){
    return unicode2utf8(wline,0,wstrlen(wline));
  }

  unsigned int unicode2utf8(const wchar_t * wline,char * line,unsigned int n){
    if (!wline){
      if (line) line[0]=0;
      return 0;
    }

    unsigned int j = ConvertUTF16toUTF8(
      (UTF16*)wline, ((wline + n) < wline) ? (const UTF16*)~0 : (const UTF16*)(wline + n),
      (UTF8*)line, (UTF8*)-1,
      lenientConversion);

    if (line) line[j]=0;

    return j;
  }

  // Binary archive format for a gen:
  // 8 bytes=the gen itself (i.e. type, subtype, etc.)
  // Additionnally for pointer types
  // 4 bytes = total size of additionnal data
  // _CPLX: both real and imaginary parts
  // _FRAC: numerator and denominator
  // _MOD: 2 gens
  // _REAL, _ZINT: long int/real binary archive
  // _VECT: 4 bytes = #rows #cols (#cols=0 if not a matrix) + list of elements
  // _SYMB: feuille + sommet
  // _FUNC: 2 bytes = -1 + string or index
  // _IDNT or _STRNG: the name
  // count number of bytes required to save g in a file
  static size_t countfunction(void const* p, size_t nbBytes,size_t NbElements, void *file)
  {
    (*(unsigned *)file)+= unsigned(nbBytes*NbElements);
    return nbBytes*NbElements;
  }
  unsigned archive_count(const gen & g,GIAC_CONTEXT){
    unsigned size= 0;
    archive_save((void*)&size, g, countfunction, contextptr, true);
    return size;
  }

  /*
  unsigned archive_count(const gen & g,GIAC_CONTEXT){
    if (g.type<=_DOUBLE_ || g.type==_FLOAT_)
      return sizeof(gen);
    if (g.type==_CPLX)
      return sizeof(gen)+sizeof(unsigned)+archive_count(*g._CPLXptr,contextptr)+archive_count(*(g._CPLXptr+1),contextptr);
    if (g.type==_REAL || g.type==_ZINT)
      return sizeof(gen)+sizeof(unsigned)+g.print(contextptr).size();
    if (g.type==_FRAC)
      return sizeof(gen)+sizeof(unsigned)+archive_count(g._FRACptr->num,contextptr)+archive_count(g._FRACptr->den,contextptr);
    if (g.type==_MOD)
      return sizeof(gen)+sizeof(unsigned)+archive_count(*g._MODptr,contextptr)+archive_count(*(g._MODptr+1),contextptr);
    if (g.type==_VECT){
      unsigned res=sizeof(gen)+sizeof(unsigned)+4;
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it)
	res += archive_count(*it,contextptr);
      return res;
    }
    if (g.type==_SYMB){
      if (archive_function_index(g._SYMBptr->sommet)) // ((equalposcomp(archive_function_tab(),g._SYMBptr->sommet))
	return sizeof(gen)+sizeof(unsigned)+sizeof(short)+archive_count(g._SYMBptr->feuille,contextptr);
      return sizeof(gen)+sizeof(unsigned)+sizeof(short)+archive_count(g._SYMBptr->feuille,contextptr)+strlen(g._SYMBptr->sommet.ptr()->s);
    }
    if (g.type==_IDNT)
      return sizeof(gen)+sizeof(unsigned)+strlen(g._IDNTptr->id_name);
    if (g.type==_FUNC){
      if (archive_function_index(*g._FUNCptr)) // (equalposcomp(archive_function_tab(),*g._FUNCptr))
	return sizeof(gen)+sizeof(unsigned)+sizeof(short);
      return sizeof(gen)+sizeof(unsigned)+sizeof(short)+strlen(g._FUNCptr->ptr()->s);
    }
    return sizeof(gen)+sizeof(unsigned)+strlen(g.print().c_str()); // not handled
  }
  */

#define DBG_ARCHIVE 0

  bool archive_save(void * f,const gen & g,size_t writefunc(void const* p, size_t nbBytes,size_t NbElements, void *file),GIAC_CONTEXT, bool noRecurse){
    // write the gen first
    writefunc(&g,sizeof(gen),1,f);
    if (g.type<=_DOUBLE_ || g.type==_FLOAT_)
      return true;
    // heap allocated object, find size
    unsigned size=0;
    if (!noRecurse) size=archive_count(g,contextptr);
    writefunc(&size,sizeof(unsigned),1,f);
    if (g.type==_CPLX)
      return archive_save(f,*g._CPLXptr,writefunc,contextptr,noRecurse) && archive_save(f,*(g._CPLXptr+1),writefunc,contextptr,noRecurse);
    if (g.type==_MOD)
      return archive_save(f,*g._MODptr,writefunc,contextptr,noRecurse) && archive_save(f,*(g._MODptr+1),writefunc,contextptr,noRecurse);
    if (g.type==_FRAC)
      return archive_save(f,g._FRACptr->num,writefunc,contextptr,noRecurse) && archive_save(f,g._FRACptr->den,writefunc,contextptr,noRecurse);
    if (g.type==_VECT){
      unsigned short rows=g._VECTptr->size(),cols=0;
      if (ckmatrix(g))
	cols=g._VECTptr->front()._VECTptr->size();
      writefunc(&rows,sizeof(short),1,f);
      writefunc(&cols,sizeof(short),1,f);
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	if (!archive_save(f,*it,writefunc,contextptr,noRecurse))
	  return false;
      }
      return true;
    }
    if (g.type==_IDNT){
#if DBG_ARCHIVE
      std::ofstream ofs;
      ofs.open ("e:\\tmp\\logsave", std::ofstream::out | std::ofstream::app);
      ofs << "IDNT " << g << '\n';
      ofs.close();
#endif
      // fprintf(f,"%s",g._IDNTptr->id_name);
      writefunc(g._IDNTptr->id_name,1,strlen(g._IDNTptr->id_name),f);
      return true;
    }
    if (g.type==_SYMB){
      if (!archive_save(f,g._SYMBptr->feuille,writefunc,contextptr,noRecurse))
	return false;
#if DBG_ARCHIVE
      std::ofstream ofs;
      ofs.open ("e:\\tmp\\logsave", std::ofstream::out | std::ofstream::app);
      ofs << "SYMB " << g << '\n';
      ofs.close();
#endif
      short i=archive_function_index(g._SYMBptr->sommet); // equalposcomp(archive_function_tab(),g._SYMBptr->sommet);
      writefunc(&i,sizeof(short),1,f);
      if (i)
	return true;
      // fprintf(f,"%s",g._SYMBptr->sommet.ptr()->s);
      writefunc(g._SYMBptr->sommet.ptr()->s,1,strlen(g._SYMBptr->sommet.ptr()->s),f);
      return true;
    }
    if (g.type==_FUNC){
      short i=archive_function_index(*g._FUNCptr); // equalposcomp(archive_function_tab(),*g._FUNCptr);
      writefunc(&i,sizeof(short),1,f);
      if (!i){
	// fprintf(f,"%s",g._FUNCptr->ptr()->s);
	writefunc(g._FUNCptr->ptr()->s,1,strlen(g._FUNCptr->ptr()->s),f);
      }
      return true;
    }
    string s;
    if (g.type==_ZINT)
      s=hexa_print_ZINT(*g._ZINTptr);
    else
      s=g.print(contextptr);
    // fprintf(f,"%s",s.c_str());
    writefunc(s.c_str(),1,s.size(),f);
    return true;
    //return false;
  }


  bool archive_save(void * f,const gen & g,GIAC_CONTEXT){
    return archive_save(f,g,(size_t (*)(void const* p, size_t nbBytes,size_t NbElements, void *file))fwrite,contextptr);
  }

#ifdef GIAC_HAS_STO_38
  // return true/false to tell if s is recognized. return the appropriate gen if true
  int lexerCompare(void const *a, void const *b) { 
    charptr_gen_unary const * ptr=(charptr_gen_unary const *)b;
    const char * aptr=(const char *) a;
    return strcmp(aptr, ptr->s); 
  }

  bool casbuiltin(const char *s, gen &g){
    // binary search in builtin_lexer_functions
#if 1 // def SMARTPTR64
    int n=builtin_lexer_functions_number; 
    charptr_gen_unary const * f = (charptr_gen_unary const *)bsearch(s, builtin_lexer_functions, n, sizeof(builtin_lexer_functions[0]), lexerCompare);
    if (f != NULL) {
      g = 0;
      int pos = int(f - builtin_lexer_functions);
      size_t val = builtin_lexer_functions_[pos];
      unary_function_ptr * at_val = (unary_function_ptr *)val;
      g = at_val;
      if (builtin_lexer_functions[pos]._FUNC_%2){
#ifdef SMARTPTR64
	unary_function_ptr tmp=*at_val;
	tmp._ptr+=1;
	g=tmp;
#else
	g._FUNC_ +=1;
#endif // SMARTPTR64
      }
      return true;
    }
#else
    charptr_gen_unary const * f = (charptr_gen_unary const *)bsearch(s, builtin_lexer_functions, builtin_lexer_functions_number, sizeof(builtin_lexer_functions[0]), lexerCompare);
    if (f != NULL) {
      g = gen(0);
      int pos=f - builtin_lexer_functions;
      *(size_t *)(&g) = builtin_lexer_functions_[pos] + f->_FUNC_;
      g = gen(*g._FUNCptr);
      return true;
    }
#endif
    if (strlen(s)==1 && s[0]==':'){
      g=at_deuxpoints;
      return true;
    }
    return false;
  }

#endif

  // restore a gen from an opened file
  gen archive_restore(void * f,size_t readfunc(void * p, size_t nbBytes,size_t NbElements, void *file),GIAC_CONTEXT){
    gen g;
    if (!readfunc(&g,sizeof(gen),1,f))
      return undef;
    if (g.type<=_DOUBLE_ || g.type==_FLOAT_)
      return g;
    unsigned char t=g.type;
    signed char s=g.subtype;
    g.type=0; // required to avoid destructor of g to mess up the pointer part
    unsigned size;
    if (!readfunc(&size,sizeof(unsigned),1,f))
      return undef;
    if (t==_CPLX || t==_MOD || t==_FRAC){
      gen g1=archive_restore(f,readfunc,contextptr);
      gen g2=archive_restore(f,readfunc,contextptr);
      if (t==_CPLX)
	return g1+cst_i*g2;
      if (t==_FRAC)
	return fraction(g1,g2);
      if (t==_MOD)
	return makemodquoted(g1,g2);
    }
    size -= sizeof(gen)+sizeof(unsigned); // adjust for gen size and length
    if (t==_VECT){
      unsigned short rows,cols;
      if (!readfunc(&rows,sizeof(unsigned short),1,f))
	return undef;
      if (!readfunc(&cols,sizeof(unsigned short),1,f))
	return undef;
//      if (!rows) return undef;
      vecteur v(rows);
      for (int i=0;i<rows;++i)
	v[i]=archive_restore(f,readfunc,contextptr);
      return gen(v,s);
    }
    if (t==_IDNT){
      char * ch=new char[size+1];
      ch[size]=0;
      if (readfunc(ch,1,size,f)!=size){
	delete [] ch;
	return undef;
      }
      string sch(ch); gen res;
      delete [] ch;
      lock_syms_mutex();
      sym_string_tab::const_iterator it=syms().find(sch),itend=syms().end();
      if (it!=itend)
	res=it->second;
      else {
	res=identificateur(sch);
	syms()[sch]=res;
      }
      unlock_syms_mutex();  
#if DBG_ARCHIVE
      std::ofstream ofs;
      ofs.open ("e:\\tmp\\logrestore", std::ofstream::out | std::ofstream::app);
      ofs << "IDNT " << res << '\n';
      ofs.close();
#endif
      return res;
    }
    if (t==_SYMB){
      gen fe=archive_restore(f,readfunc,contextptr);
      short index;
      if (!readfunc(&index,sizeof(short),1,f))
	return undef;
      if (index>0){
	const unary_function_ptr * aptr=archive_function_tab();
	if (index<archive_function_tab_length){
	  g=symbolic(aptr[index-1],fe);
	}
	else {
#if DBG_ARCHIVE
	  std::ofstream ofs;
	  ofs.open ("e:\\tmp\\logrestore", std::ofstream::out | std::ofstream::app);
	  ofs << "archive_restore error _SYMB index " << index << '\n';
	  ofs.close();
#endif
	  g=fe; // ERROR
	}
      }
      else {
	gen res;
	size -= archive_count(fe,contextptr)+sizeof(short);
	char * ch=new char[size+3];
	if (0 && abs_calc_mode(contextptr)==38){
	  ch[0]='\'';
	  ch[size+1]='\'';
	  ch[size+2]=0;
	  if (readfunc(ch+1,1,size,f)!=size){
	    delete [] ch;
	    return undef;
	  }
	}
	else {
	  ch[size]=0;
	  if (readfunc(ch,1,size,f)!=size){
	    delete [] ch;
	    return undef;
	  }
#if defined NSPIRE
	  if (builtin_lexer_functions_()){
	    std::pair<charptr_gen *,charptr_gen *> p=equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(ch,0),tri);
	    if (p.first!=p.second && p.first!=builtin_lexer_functions_end()){
	      res = p.first->second;
	      res.subtype=1;
	      res=gen(int((*builtin_lexer_functions_())[p.first-builtin_lexer_functions_begin()]+p.first->second.val));
	      res=gen(*res._FUNCptr);
	    }
	  }
#else	  
	  if (builtin_lexer_functions_){
#ifdef GIAC_HAS_STO_38
	    if (!casbuiltin(ch,res)){
#if DBG_ARCHIVE
	      std::ofstream ofs;
	      ofs.open ("e:\\tmp\\logrestore", std::ofstream::out | std::ofstream::app);
	      ofs << "archive_restore error _SYMB " << ch << '\n';
	      ofs.close();
#endif
	      res=0;
	    }
#else
	    std::pair<charptr_gen *,charptr_gen *> p=equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(ch,0),tri);
	    if (p.first!=p.second && p.first!=builtin_lexer_functions_end()){
	      res = p.first->second;
	      res.subtype=1;
	      res=gen(int(builtin_lexer_functions_[p.first-builtin_lexer_functions_begin()]+p.first->second.val));
	      res=gen(*res._FUNCptr);
	    }
#endif
	  }
#endif 
	}
	if (is_zero(res)){
#if DBG_ARCHIVE
	  std::ofstream ofs;
	  ofs.open ("e:\\tmp\\logrestore", std::ofstream::out | std::ofstream::app);
	  ofs << "archive_restore error _SYMB 0 " << ch << '\n';
	  ofs.close();
#endif
	  res=gen(ch,contextptr);
	}
	delete [] ch;
	if (res.type!=_FUNC){
	  return undef;
	}
	g=symbolic(*res._FUNCptr,fe);
      }
      g.subtype=s;
#if DBG_ARCHIVE
      std::ofstream ofs;
      ofs.open ("e:\\tmp\\logrestore", std::ofstream::out | std::ofstream::app);
      ofs << "SYMB " << g << '\n';
      ofs.close();
#endif
      return g;
    }
    if (t==_FUNC){
      short index;
      if (!readfunc(&index,sizeof(short),1,f))
	return undef;
      if (index>0)
	g = archive_function_tab()[index-1];
      else {
	size -= sizeof(short);
	char * ch=new char[size+1];
	ch[size]=0;
	if (readfunc(ch,1,size,f)!=size){
	  delete [] ch;
	  return undef;
	}
	g = gen(ch,contextptr);
	delete [] ch;
	if (g.type!=_FUNC)
	  return undef;
      }
      g.subtype=s;
      return g;
    }
    char * ch=new char[size+1];
    ch[size]=0;
    if (readfunc(ch,1,size,f)!=size){
      delete [] ch;
      return undef;
    }
    gen res;
    if (t==_STRNG)
      res=string2gen(ch,true);
    else
      res=gen(ch,contextptr);
    delete [] ch;
    return res;
  }

  gen archive_restore(FILE * f,GIAC_CONTEXT){
    return archive_restore(f,(size_t (*)(void * p, size_t nbBytes,size_t NbElements, void *file))fread,contextptr);
  }

  void init_geogebra(bool on,GIAC_CONTEXT){
#ifndef FXCG
    setlocale(LC_NUMERIC,"POSIX");
#endif
    _decimal_digits_=on?13:12;
    _all_trig_sol_=on;
    _withsqrt_=!on;
    _calc_mode_=on?1:0;
    _eval_equaltosto_=on?0:1;
    eval_equaltosto(on?0:1,contextptr);
    decimal_digits(on?13:12,contextptr);
    all_trig_sol(on,contextptr);
    withsqrt(!on,contextptr);
    calc_mode(on?1:0,contextptr);
    powlog2float=3e4;
    MPZ_MAXLOG2=33300;
#ifdef TIMEOUT
    //caseval_maxtime=5;
    caseval_n=0;
    caseval_mod=10;
#endif
  }

  vecteur giac_current_status(bool save_history,GIAC_CONTEXT){
    // cas and geo config
    vecteur res;
    if (abs_calc_mode(contextptr)==38)
      res.push_back(cas_setup(contextptr));
    else
      res.push_back(symbolic(at_cas_setup,cas_setup(contextptr)));
    res.push_back(xyztrange(gnuplot_xmin,gnuplot_xmax,gnuplot_ymin,gnuplot_ymax,gnuplot_zmin,gnuplot_zmax,gnuplot_tmin,gnuplot_tmax,global_window_xmin,global_window_xmax,global_window_ymin,global_window_ymax,show_axes(contextptr),class_minimum,class_size,
#ifdef WITH_GNUPLOT
			    gnuplot_hidden3d,gnuplot_pm3d
#else
			    1,1
#endif
			    ));
    if (abs_calc_mode(contextptr)==38)
      res.back()=res.back()._SYMBptr->feuille;
    // session
    res.push_back(save_history?history_in(contextptr):vecteur(0));
    res.push_back(save_history?history_out(contextptr):vecteur(0));
    // user variables
    if (contextptr && contextptr->tabptr){
      sym_tab::const_iterator jt=contextptr->tabptr->begin(),jtend=contextptr->tabptr->end();
      for (;jt!=jtend;++jt){
	gen a=jt->second;
	gen b=identificateur(jt->first);
	res.push_back(symb_sto(a,b));
      }
    }
    else {
      lock_syms_mutex();  
      sym_string_tab::const_iterator it=syms().begin(),itend=syms().end();
      for (;it!=itend;++it){
	gen id=it->second;
	if (id.type==_IDNT && id._IDNTptr->value && id._IDNTptr->ref_count_ptr!=(int *) -1)
	  res.push_back(symb_sto(*id._IDNTptr->value,id));
      }
      unlock_syms_mutex();  
    }
    int xc=xcas_mode(contextptr);
    if (xc==0 && python_compat(contextptr))
      xc=256*python_compat(contextptr);
    if (abs_calc_mode(contextptr)==38)
      res.push_back(xc);
    else
      res.push_back(symbolic(at_xcas_mode,xc));
    return res;
  }

  bool unarchive_session(const gen & g,int level,const gen & replace,GIAC_CONTEXT,bool with_history){
    int l;
    if (g.type!=_VECT || (l=int(g._VECTptr->size()))<4)
      return false;
    vecteur v=*g._VECTptr;
    if (v[2].type!=_VECT || v[3].type!=_VECT || (v[2]._VECTptr->size()!=v[3]._VECTptr->size() && v[2]._VECTptr->size()!=v[3]._VECTptr->size()+1))
      return false;
    if (v[2]._VECTptr->size()==v[3]._VECTptr->size()+1)
      v[2]._VECTptr->pop_back();
#ifndef DONT_UNARCHIVE_HISTORY
    history_in(contextptr)=*v[2]._VECTptr;
    history_out(contextptr)=*v[3]._VECTptr;
#ifndef GNUWINCE
    if (v[0].type==_VECT)
      _cas_setup(v[0],contextptr);
    else
      protecteval(v[0],eval_level(contextptr),contextptr); 
    if (v[1].type==_VECT)
      _xyztrange(v[1],contextptr);
    else
      protecteval(v[1],eval_level(contextptr),contextptr); 
#endif
#endif
    // restore variables
    for (int i=4;i<l;++i)
      protecteval(v[i],eval_level(contextptr),contextptr); 
    // restore xcas_mode
    if (v.back().type==_INT_)
      xcas_mode(v.back().val,contextptr);
    if (!with_history)
      return true;
    // eval replace if level>=0
    if (level<0 || level>=l){
      history_in(contextptr).push_back(replace);
      history_out(contextptr).push_back(protecteval(replace,eval_level(contextptr),contextptr)); 
    }
    else {
      history_in(contextptr)[level]=replace;
      for (int i=level;i<l;++i)
	history_out(contextptr)[i]=protecteval(history_in(contextptr)[i],eval_level(contextptr),contextptr); 
    }
    return true;
  }

  const char * const do_not_autosimplify[]={
    "Factor",
    "Gcd",
    "Int",
    "POLYFORM",
    "Quo",
    "Quorem",
    "Rem",
    "animate",
    "animation",
    "archive",
    "autosimplify",
    "canonical_form",
    "cfactor",
    "convert",
    "cpartfrac",
    "curve",
    "developper",
    "diff",
    "domain",
    "element",
    "evalc",
    "expand",
    "expexpand",
    "factor",
    "factoriser",
    "factoriser_entier",
    "factoriser_sur_C",
    "ifactor",
    "list2exp",
    "lncollect",
    "lnexpand",
    "mathml",
    "mult_c_conjugate",
    "mult_conjugate",
    "multiplier_conjugue",
    "nodisp",
    "normal",
    "op",
    "partfrac",
    "plotfield",
    "plotfunc",
    "plotparam",
    "plotpolar",
    "pow2exp",
    "powexpand",
    "propFrac",
    "propfrac",
    "quote",
    "regroup",
    "reorder",
    "series",
    "simplifier",
    "simplify",
    "tabvar",
    "taylor",
    "texpand",
    "trace",
    "trigexpand",
    0
  };

  int dichotomic_search(const char * const * tab,unsigned tab_size,const char * s){
    int beg=0,end=tab_size,cur,test;
    // string index is always >= begin and < end
    for (;;){
      cur=(beg+end)/2;
      test=strcmp(s,tab[cur]);
      if (!test)
	return cur;
      if (cur==beg)
	return -1;
      if (test>0)
	beg=cur;
      else
	end=cur;
    }
    return -1;
  }

  gen add_autosimplify(const gen & g,GIAC_CONTEXT){
    if (g.type==_VECT)
      return apply(g,add_autosimplify,contextptr);
    if (g.type==_SYMB){
      if (g._SYMBptr->sommet==at_program)
	return g;
#ifdef GIAC_HAS_STO_38
      const char * c=g._SYMBptr->sommet.ptr()->s;
#else
      string ss=g._SYMBptr->sommet.ptr()->s;
      if (g._SYMBptr->sommet==at_sto && g._SYMBptr->feuille.type==_VECT){
	vecteur & v=*g._SYMBptr->feuille._VECTptr;
	if (v.size()==2 && v.front().type==_SYMB)
	  ss=v.front()._SYMBptr->sommet.ptr()->s;
      }
      ss=unlocalize(ss);
      const char * c=ss.c_str();
#endif
#if 1
      if (dichotomic_search(do_not_autosimplify,sizeof(do_not_autosimplify)/sizeof(char*)-1,c)!=-1)
	return g;
#else
      const char ** ptr=do_not_autosimplify;
      for (;*ptr;++ptr){
	if (!strcmp(*ptr,c))
	  return g;
      }
#endif
    }
    std::string s=autosimplify(contextptr);
    if (s.size()<1 || s=="'nop'")
      return g;
    gen a(s,contextptr);
    if (a.type==_FUNC)
      return symbolic(*a._FUNCptr,g);
    if (a.type>=_IDNT)
      return symb_of(a,g);
    return g;
  }

  bool csv_guess(const char * data,int count,char & sep,char & nl,char & decsep){
    bool ans=true;
    int nb[256],pointdecsep=0,commadecsep=0; 
    for (int i=0;i<256;++i)
      nb[i]=0;
    // count occurrence of each char
    // and detect decimal separator between . or ,
    for (int i=1;i<count-1;++i){
      if (data[i]=='[' || data[i]==']')
	ans=false;
      ++nb[(unsigned char) data[i]];
      if (data[i-1]>='0' && data[i-1]<='9' && data[i+1]>='0' && data[i+1]<='9'){
	if (data[i]=='.')
	  ++pointdecsep;
	if (data[i]==',')
	  ++commadecsep;
      }
    }
    decsep=commadecsep>pointdecsep?',':'.';
    // detect nl (ctrl-M or ctrl-J)
    nl=nb[10]>nb[13]?10:13;
    // find in control characters and : ; the most used (except 10/13)
    int nbmax=0,imax=-1;
    for (int i=0;i<60;++i){
      if (i==10 || i==13 || (i>=' ' && i<='9') )
	continue;
      if (nb[i]>nbmax){
	imax=i;
	nbmax=nb[i];
      }
    }
    // compare . with , (44)
    if (nb[unsigned(',')] && nb[unsigned(',')]>=nbmax){
      imax=',';
      nbmax=nb[unsigned(',')];
    }
    if (nbmax && nbmax>=nb[unsigned(nl)] && imax!=decsep)
      sep=imax;
    else
      sep=' ';
    return ans;
  }

  void (*my_gprintf)(unsigned special,const string & format,const vecteur & v,GIAC_CONTEXT)=0;


#if defined(EMCC) || defined(EMCC2)
  static void newlinestobr(string &s,const string & add){
    int l=int(add.size());
    for (int i=0;i<l;++i){
      if (add[i]=='\n')
	s+="<br>";
      else
	s+=add[i];
    }
  }
#else
  static void newlinestobr(string &s,const string & add){
    s+=add;
  }
#endif

  void gprintf(unsigned special,const string & format,const vecteur & v,GIAC_CONTEXT){
    return gprintf(special,format,v,step_infolevel(contextptr),contextptr);
  }

  void gprintf(unsigned special,const string & format,const vecteur & v,int step_info,GIAC_CONTEXT){
    if (step_info==0)
      return;
    if (my_gprintf){
      my_gprintf(special,format,v,contextptr);
      return;
    }
    string s;
    int pos=0;
#if defined(EMCC) || defined(EMCC2)
    *logptr(contextptr) << char(2) << '\n'; // start mixed text/mathml
#endif
    for (unsigned i=0;i<v.size();++i){
      int p=int(format.find("%gen",pos));
      if (p<0 || p>=int(format.size()))
	break;
      newlinestobr(s,format.substr(pos,p-pos));
#if defined(EMCC) || defined(EMCC2)
      gen tmp;
      if (v[i].is_symb_of_sommet(at_pnt))
	tmp=_svg(v[i],contextptr);
      else
	tmp=_mathml(makesequence(v[i],1),contextptr);
      s = s+((tmp.type==_STRNG)?(*tmp._STRNGptr):v[i].print(contextptr));
#else
      s += v[i].print(contextptr);
#endif
      pos=p+4;
    }
    newlinestobr(s,format.substr(pos,format.size()-pos));
    *logptr(contextptr) << s << '\n';
#if defined(EMCC) || defined(EMCC2)
    *logptr(contextptr) << char(3) << '\n'; // end mixed text/mathml
    *logptr(contextptr) << '\n';
#endif
  }

  void gprintf(const string & format,const vecteur & v,GIAC_CONTEXT){
    gprintf(step_nothing_special,format,v,contextptr);
  }

  void gprintf(const string & format,const vecteur & v,int step_info,GIAC_CONTEXT){
    gprintf(step_nothing_special,format,v,step_info,contextptr);
  }

  // moved from input_lexer.ll for easier debug
  const char invalid_name[]="Invalid name";

#if defined USTL || defined GIAC_HAS_STO_38 || (defined KHICAS && !defined(SIMU)) || defined SDL_KHICAS
#if defined GIAC_HAS_STO_38 || defined KHICAS || defined SDL_KHICAS
void update_lexer_localization(const std::vector<int> & v,std::map<std::string,std::string> &lexer_map,std::multimap<std::string,localized_string> &back_lexer_map,GIAC_CONTEXT){}
#endif
#else
  vecteur * keywords_vecteur_ptr(){
    static vecteur v;
    return &v;
  }

  static void in_update_lexer_localization(istream & f,int lang,const std::vector<int> & v,std::map<std::string,std::string> &lexer_map,std::multimap<std::string,localized_string> &back_lexer_map,GIAC_CONTEXT){
    char * line = (char *)malloc(1024);
    std::string giac_kw,local_kw;
    size_t l;
    for (;;){
      f.getline(line,1023,'\n');
      l=strlen(line);
      if (f.eof()){
	break;
      }
      if (l>3 && line[0]!='#'){
	if (line[l-1]=='\n')
	  --l;
	// read giac keyword
	size_t j;
	giac_kw="";
	for (j=0;j<l;++j){
	  if (line[j]==' ')
	    break;
	  giac_kw += line[j];
	}
	// read corresponding local keywords
	local_kw="";
	vecteur * keywordsptr=keywords_vecteur_ptr();
	for (++j;j<l;++j){
	  if (line[j]==' '){
	    if (!local_kw.empty()){
#if defined(EMCC) || defined(EMCC2)
	      gen localgen(gen(local_kw,contextptr));
	      keywordsptr->push_back(localgen);
	      sto(gen(giac_kw,contextptr),localgen,contextptr);
#else
	      lexer_map[local_kw]=giac_kw;
	      back_lexer_map.insert(pair<string,localized_string>(giac_kw,localized_string(lang,local_kw)));
#endif
	    }
	    local_kw="";
	  }
	  else
	    local_kw += line[j];
	}
	if (!local_kw.empty()){
#if defined(EMCC) || defined(EMCC2)
	  gen localgen(gen(local_kw,contextptr));
	  keywordsptr->push_back(localgen);
	  sto(gen(giac_kw,contextptr),localgen,contextptr);
#else
	  lexer_map[local_kw]=giac_kw;
	  back_lexer_map.insert(pair<string,localized_string>(giac_kw,localized_string(lang,local_kw)));
#endif
	}
      }
    }
    free(line);
  }
	    
  void update_lexer_localization(const std::vector<int> & v,std::map<std::string,std::string> &lexer_map,std::multimap<std::string,localized_string> &back_lexer_map,GIAC_CONTEXT){
    lexer_map.clear();
    back_lexer_map.clear();
    int s=int(v.size());
    for (int i=0;i<s;++i){
      int lang=v[i];
      if (lang>=1 && lang<=4){
	std::string doc=find_doc_prefix(lang);
	std::string file=giac_aide_dir()+doc+"keywords";
	//COUT << "keywords " << file << '\n';
	ifstream f(file.c_str());
	if (f.good()){
	  in_update_lexer_localization(f,lang,v,lexer_map,back_lexer_map,contextptr);
	  // COUT << "// Using keyword file " << file << '\n';
	} // if (f)
	else {
	  if (lang==1){
#ifdef HAVE_SSTREAM
      istringstream f(
#else
      istrstream f(
#endif
		   "# enter couples\n# giac_keyword translation\n# for example, to define integration as a translation for integrate \nintegrate integration\neven est_pair\nodd est_impair\n# geometry\nbarycenter barycentre\nisobarycenter isobarycentre\nmidpoint milieu\nline_segments aretes\nmedian_line mediane\nhalf_line demi_droite\nparallel parallele\nperpendicular perpendiculaire\ncommon_perpendicular perpendiculaire_commune\nenvelope enveloppe\nequilateral_triangle triangle_equilateral\nisosceles_triangle triangle_isocele\nright_triangle triangle_rectangle\nlocus lieu\ncircle cercle\nconic conique\nreduced_conic conique_reduite\nquadric quadrique\nreduced_quadric quadrique_reduite\nhyperbola hyperbole\ncylinder cylindre\nhalf_cone demi_cone\nline droite\nplane plan\nparabola parabole\nrhombus losange\nsquare carre\nhexagon hexagone\npyramid pyramide\nquadrilateral quadrilatere\nparallelogram parallelogramme\northocenter orthocentre\nexbisector exbissectrice\nparallelepiped parallelepipede\npolyhedron polyedre\ntetrahedron tetraedre\ncentered_tetrahedron tetraedre_centre\ncentered_cube cube_centre\noctahedron octaedre\ndodecahedron dodecaedre\nicosahedron icosaedre\nbisector bissectrice\nperpen_bisector mediatrice\naffix affixe\naltitude hauteur\ncircumcircle circonscrit\nexcircle exinscrit\nincircle inscrit\nis_prime est_premier\nis_equilateral est_equilateral\nis_rectangle est_rectangle\nis_parallel est_parallele\nis_perpendicular est_perpendiculaire\nis_orthogonal est_orthogonal\nis_collinear est_aligne\nis_concyclic est_cocyclique\nis_element est_element\nis_included est_inclus\nis_coplanar est_coplanaire\nis_isosceles est_isocele\nis_square est_carre\nis_rhombus est_losange\nis_parallelogram est_parallelogramme\nis_conjugate est_conjugue\nis_harmonic_line_bundle est_faisceau_droite\nis_harmonic_circle_bundle est_faisceau_cercle\nis_inside est_dans\narea aire\nperimeter perimetre\ndistance longueur\ndistance2 longueur2\nareaat aireen\nslopeat penteen\nangleat angleen\nperimeterat perimetreen\ndistanceat distanceen\nareaatraw aireenbrut\nslopeatraw penteenbrut\nangleatraw angleenbrut\nperimeteratraw perimetreenbrut\ndistanceatraw distanceenbrut\nextract_measure extraire_mesure\ncoordinates coordonnees\nabscissa abscisse\nordinate ordonnee\ncenter centre\nradius rayon\npowerpc puissance\nvertices sommets\npolygon polygone\nisopolygon isopolygone\nopen_polygon polygone_ouvert\nhomothety homothetie\nsimilarity similitude\n# affinity affinite\nreflection symetrie\nreciprocation polaire_reciproque\nscalar_product produit_scalaire\n# solid_line ligne_trait_plein\n# dash_line ligne_tiret\n# dashdot_line ligne_tiret_point\n# dashdotdot_line ligne_tiret_pointpoint\n# cap_flat_line ligne_chapeau_plat\n# cap_round_line ligne_chapeau_rond\n# cap_square_line ligne_chapeau_carre\n# line_width_1 ligne_epaisseur_1\n# line_width_2 ligne_epaisseur_2\n# line_width_3 ligne_epaisseur_3\n# line_width_4 ligne_epaisseur_4\n# line_width_5 ligne_epaisseur_5\n# line_width_6 ligne_epaisseur_6\n# line_width_7 ligne_epaisseur_7\n# line_width_8 ligne_epaisseur_8\n# rhombus_point point_losange\n# plus_point point_plus\n# square_point point_carre\n# cross_point point_croix\n# triangle_point point_triangle\n# star_point point_etoile\n# invisible_point point_invisible\ncross_ratio birapport\nradical_axis axe_radical\npolar polaire\npolar_point point_polaire\npolar_coordinates coordonnees_polaires\nrectangular_coordinates coordonnees_rectangulaires\nharmonic_conjugate conj_harmonique\nharmonic_division div_harmonique\ndivision_point point_div\n# harmonic_division_point point_division_harmonique\ndisplay affichage\nvertices_abc sommets_abc\nvertices_abca sommets_abca\nline_inter inter_droite\nsingle_inter inter_unique\ncolor couleur\nlegend legende\nis_harmonic est_harmonique\nbar_plot diagramme_batons\nbarplot diagrammebatons\nhistogram histogramme\nprism prisme\nis_cospherical est_cospherique\ndot_paper papier_pointe\ngrid_paper papier_quadrille\nline_paper papier_ligne\ntriangle_paper papier_triangule\nvector vecteur\nplotarea tracer_aire\nplotproba graphe_probabiliste\nmult_c_conjugate mult_conjugue_C\nmult_conjugate mult_conjugue\ncanonical_form forme_canonique\nibpu integrer_par_parties_u\nibpdv integrer_par_parties_dv\nwhen quand\nslope pente\ntablefunc table_fonction\ntableseq table_suite\nfsolve resoudre_numerique\ninput saisir\nprint afficher\nassume supposons\nabout domaine\nbreakpoint point_arret\nwatch montrer\nrmwatch ne_plus_montrer\nrmbreakpoint suppr_point_arret\nrand alea\nInputStr saisir_chaine\nOx_2d_unit_vector vecteur_unitaire_Ox_2d\nOy_2d_unit_vector vecteur_unitaire_Oy_2d\nOx_3d_unit_vector vecteur_unitaire_Ox_3d\nOy_3d_unit_vector vecteur_unitaire_Oy_3d\nOz_3d_unit_vector vecteur_unitaire_Oz_3d\nframe_2d repere_2d\nframe_3d repere_3d\nrsolve resoudre_recurrence\nassume supposons\ncumulated_frequencies frequences_cumulees\nfrequencies frequences\nnormald loi_normale\nregroup regrouper\nosculating_circle cercle_osculateur\ncurvature courbure\nevolute developpee\nvector vecteur\n");
	    in_update_lexer_localization(f,1,v,lexer_map,back_lexer_map,contextptr);
	  }
	  else
	    CERR << "// Unable to find keyword file " << file << '\n';
	}
      }
    }
  }
#endif

#if !defined NSPIRE 

#include "input_parser.h" 

    bool has_special_syntax(const char * s){
#ifdef USTL
      ustl::pair<charptr_gen *,charptr_gen *> p=
	ustl::equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),
		    std::pair<const char *,gen>(s,0),
		    tri);
#else
      std::pair<charptr_gen *,charptr_gen *> p=
	equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),
		    std::pair<const char *,gen>(s,0),
		    tri);
#endif
      if (p.first!=p.second && p.first!=builtin_lexer_functions_end())
	return (p.first->second.subtype!=T_UNARY_OP-256);
      map_charptr_gen::const_iterator i = lexer_functions().find(s);
      if (i==lexer_functions().end())
	return false;
      return (i->second.subtype!=T_UNARY_OP-256);
    }
    
    bool lexer_functions_register(const unary_function_ptr & u,const char * s,int parser_token){
      map_charptr_gen::const_iterator i = lexer_functions().find(s);
      if (i!=lexer_functions().end())
	return false;
      if (doing_insmod){
	if (debug_infolevel) CERR << "insmod register " << s << '\n';
	registered_lexer_functions().push_back(user_function(s,parser_token));
      }
      if (!builtin_lexer_functions_sorted){
#ifndef STATIC_BUILTIN_LEXER_FUNCTIONS
#if defined NSPIRE_NEWLIB || defined KHICAS || defined NUMWORKS
	builtin_lexer_functions_begin()[builtin_lexer_functions_number]=std::pair<const char *,gen>(s,gen(u));
#else
	builtin_lexer_functions_begin()[builtin_lexer_functions_number].first=s;
	builtin_lexer_functions_begin()[builtin_lexer_functions_number].second.type=0;
	builtin_lexer_functions_begin()[builtin_lexer_functions_number].second=gen(u);
#endif
	if (parser_token==1)
	  builtin_lexer_functions_begin()[builtin_lexer_functions_number].second.subtype=T_UNARY_OP-256;
	else
	  builtin_lexer_functions_begin()[builtin_lexer_functions_number].second.subtype=parser_token-256;
	builtin_lexer_functions_number++;
#endif
	if (debug_infolevel) CERR << "insmod register builtin " << s << '\n';
      }
      else {
	lexer_functions()[s] = gen(u);
	if (parser_token==1)
	  lexer_functions()[s].subtype=T_UNARY_OP-256;
	else
	  lexer_functions()[s].subtype=parser_token-256;
	if (debug_infolevel) CERR << "insmod register lexer_functions " << s << '\n';	
      }
      // If s is a library function name (with ::), update the library
      int ss=int(strlen(s)),j=0;
      for (;j<ss-1;++j){
	if (s[j]==':' && s[j+1]==':')
	  break;
      }
      if (j<ss-1){
	string S(s);
	string libname=S.substr(0,j);
	string funcname=S.substr(j+2,ss-j-2);
#ifdef USTL
	ustl::map<std::string,std::vector<string> >::iterator it=library_functions().find(libname);
#else
	std::map<std::string,std::vector<string> >::iterator it=library_functions().find(libname);
#endif
	if (it!=library_functions().end())
	  it->second.push_back(funcname);
	else
	  library_functions()[libname]=vector<string>(1,funcname);
      }
      return true;
    }

    bool lexer_function_remove(const vector<user_function> & v){
      vector<user_function>::const_iterator it=v.begin(),itend=v.end();
      map_charptr_gen::const_iterator i,iend;
      bool ok=true;
      for (;it!=itend;++it){
	i = lexer_functions().find(it->s.c_str());
	iend=lexer_functions().end();
	if (i==iend)
	  ok=false;
	else
	  lexer_functions().erase(it->s.c_str());
      }
      return ok;
    }

#if defined EMCC || defined EMCC2 || defined SIMU
  bool cas_builtin(const char * s,GIAC_CONTEXT){
    std::pair<charptr_gen *,charptr_gen *> p=std::equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(s,0),tri);
    bool res=p.first!=p.second && p.first!=builtin_lexer_functions_end();
    if (res)
      return res;
    gen g;
    int token=find_or_make_symbol(s,g,0,false,contextptr);
    if (g.type!=_IDNT)
      return false;
    gen evaled;
    if (!g._IDNTptr->in_eval(1,g,evaled,contextptr,false))
      return false;
    //confirm("builtin?",evaled.print(contextptr).c_str());
    return evaled.is_symb_of_sommet(at_program);
    return res;
  }
#endif

  bool my_isalpha(char c){
    return (c>='a' && c<='z') || (c>='A' && c<='Z');
  }

  int find_or_make_symbol(const string & s,gen & res,void * scanner,bool check38,GIAC_CONTEXT){
      int tmpo=opened_quote(contextptr);
      if (tmpo & 2)
	check38=false;
      if (s.size()==1){
#ifdef GIAC_HAS_STO_38
	if (0 && s[0]>='a' && s[0]<='z'){
	  index_status(contextptr)=1; 
	  res=*tab_one_letter_idnt[s[0]-'a'];
	  return T_SYMBOL;
	}
	if (check38 && s[0]>='a' && s[0]<='z' && calc_mode(contextptr)==38)
	  giac_yyerror(scanner,invalid_name);
#else
	if (s[0]>='a' && s[0]<='z'){
	  if (check38 && calc_mode(contextptr)==38)
	    giac_yyerror(scanner,invalid_name);
	  index_status(contextptr)=1; 
	  res=*tab_one_letter_idnt[s[0]-'a'];
	  return T_SYMBOL;
	}
#endif
	switch (s[0]){
	case '+':
	  res=at_plus;
	  return T_UNARY_OP;
	case '-':
	  res=at_neg;
	  return T_UNARY_OP;
	case '*':
	  res=at_prod;
	  return T_UNARY_OP;
	case '/':
	  res=at_division;
	  return T_UNARY_OP;
	case '^':
	  res=at_pow;
	  return T_UNARY_OP;
	}
      }
      string ts(s);
#ifdef USTL
      ustl::map<std::string,std::string>::const_iterator trans=lexer_localization_map().find(ts);
      if (trans!=lexer_localization_map().end())
	ts=trans->second;
      ustl::map<std::string,std::vector<string> >::const_iterator j=lexer_translator().find(ts);
      if (j!=lexer_translator().end() && !j->second.empty())
	ts=j->second.back();
      ustl::pair<charptr_gen *,charptr_gen *> p=ustl::equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(ts.c_str(),0),tri);
#else
      std::map<std::string,std::string>::const_iterator trans=lexer_localization_map().find(ts);
      if (trans!=lexer_localization_map().end())
	ts=trans->second;
      std::map<std::string,std::vector<string> >::const_iterator j=lexer_translator().find(ts);
      if (j!=lexer_translator().end() && !j->second.empty())
	ts=j->second.back();
      std::pair<charptr_gen *,charptr_gen *> p=equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(ts.c_str(),0),tri);
#endif
      if (p.first!=p.second && p.first!=builtin_lexer_functions_end()){
	if (p.first->second.subtype==T_TO-256)
	  res=plus_one;
	else
	  res = p.first->second;
	res.subtype=1;
	if (builtin_lexer_functions_){
#ifdef NSPIRE
	  res=gen(int((*builtin_lexer_functions_())[p.first-builtin_lexer_functions_begin()]+p.first->second.val));
	  res=gen(*res._FUNCptr);	  
#else
#if !defined NSPIRE_NEWLIB || defined KHICAS
	  res=0;
	  int pos=int(p.first-builtin_lexer_functions_begin());
#if defined KHICAS && !defined SDL_KHICAS && !defined x86_64 && !defined __ARM_ARCH_ISA_A64 && !defined __MINGW_H
	  const unary_function_ptr * at_val=*builtin_lexer_functions_[pos];
#else
	  size_t val=builtin_lexer_functions_[pos];
	  unary_function_ptr * at_val=(unary_function_ptr *)val;
#endif
	  res=at_val;
#if defined GIAC_HAS_STO_38 || (defined KHICAS && defined DEVICE)
	  if (builtin_lexer_functions[pos]._FUNC_%2){
#ifdef SMARTPTR64
	    unary_function_ptr tmp=*at_val;
	    tmp._ptr+=1;
	    res=tmp;
#else
	    res._FUNC_ +=1;
#endif // SMARTPTR64
	  }
#endif // GIAC_HAS_STO_38
#else // keep this code, required for the nspire otherwise evalf(pi)=reboot
	  res=gen(int(builtin_lexer_functions_[p.first-builtin_lexer_functions_begin()]+p.first->second.val));
	  res=gen(*res._FUNCptr);
#endif
#endif
	}
	index_status(contextptr)=(p.first->second.subtype==T_UNARY_OP-256);
	int token=p.first->second.subtype;
	token += (token<0)?512:256 ;	
	return token;
      }
      lexer_tab_int_type tst={ts.c_str(),0,0,0,0};
#ifdef USTL
      ustl::pair<const lexer_tab_int_type *,const lexer_tab_int_type *> pp = ustl::equal_range(lexer_tab_int_values,lexer_tab_int_values_end,tst,tri1);
#else
      std::pair<const lexer_tab_int_type *,const lexer_tab_int_type *> pp = equal_range(lexer_tab_int_values,lexer_tab_int_values_end,tst,tri1);
#endif
      if (pp.first!=pp.second && pp.first!=lexer_tab_int_values_end){
	index_status(contextptr)=pp.first->status;
	res=int(pp.first->value);
	res.subtype=pp.first->subtype;
	return pp.first->return_value;
      }
      // CERR << "lexer_functions search " << ts << '\n';
      map_charptr_gen::const_iterator i = lexer_functions().find(ts.c_str());
      if (i!=lexer_functions().end()){
	// CERR << "lexer_functions found " << ts << '\n';
	if (i->second.subtype==T_TO-256)
	  res=plus_one;
	else
	  res = i->second;
	res.subtype=1;
	index_status(contextptr)=(i->second.subtype==T_UNARY_OP-256);
	return i->second.subtype+256 ;
      }
      lock_syms_mutex();
      sym_string_tab::const_iterator i2 = syms().find(s),i2end=syms().end();
      if (i2 == i2end) {
	unlock_syms_mutex();  
	const char * S = s.c_str();
	// std::CERR << "lexer new" << s << '\n';
	if (check38 && calc_mode(contextptr)==38 && strcmp(S,string_pi) && strcmp(S,string_euler_gamma) && strcmp(S,string_infinity) && strcmp(S,string_undef) && S[0]!='G'&& (!is_known_name_38 || !is_known_name_38(0,S))){
	  // detect invalid names and implicit multiplication 
	  size_t ss=strlen(S);
	  vecteur args;
	  for (size_t i=0;i<ss;++i){
	    char ch=S[i];
	    if (ch=='C' || (ch>='E' && ch<='H') || ch=='L' || ch=='M' || ch=='R'
		/* || ch=='S' */
		|| ch=='U' || ch=='V' || (ch>='X' && ch<='Z') ){
	      string name;
	      name += ch;
	      char c=0;
	      if (i<ss-1)
		c=s[i+1];
	      if (c>='0' && c<='9'){
		name += c;
		++i;
	      }
	      res = identificateur(name);
	      lock_syms_mutex();
	      syms()[name] = res;
	      unlock_syms_mutex();
	      args.push_back(res);
	    }
	    else {
	      string coeff;
	      for (++i;i<ss;++i){
		// up to next alphabetic char
		if (s[i]>32 && my_isalpha(s[i])){
		  --i;
		  break;
		}
		if (scanner && (s[i]<0 || s[i]>'z')){
		  giac_yyerror(scanner,invalid_name);
		  res=undef;
		  return T_SYMBOL;
		}
		coeff += s[i];
	      }
	      if (coeff.empty())
		res=1;
	      else
		res=strtod(coeff.c_str(),0);
	      if (ch=='i')
		res=res*cst_i;
	      else {
		if (ch=='e')
		  res=std::exp(1.0)*res;
		else {
		  // Invalid ident name, report error
		  if ( (ch>'Z' || ch<0) && scanner){
		    giac_yyerror(scanner,invalid_name);
		    res=undef;
		    return T_SYMBOL;
		  }
		  coeff=string(1,ch);
		  gen tmp = identificateur(coeff);
		  // syms()[coeff.c_str()]=tmp;
		  res=res*tmp;
		}
	      }
	      args.push_back(res);
	    }
	  }
	  if (args.size()==1)
	    res=args.front();
	  else 
	    res=_prod(args,contextptr);
	  lock_syms_mutex();
	  syms()[s]=res;
	  unlock_syms_mutex();
	  return T_SYMBOL;
	} // end 38 compatibility mode
	res = identificateur(s);
	lock_syms_mutex();
	syms()[s] = res;
	unlock_syms_mutex();
	return T_SYMBOL;
      } // end if ==syms.end()
      res = i2->second;
      unlock_syms_mutex();  
      return T_SYMBOL;
    }

  // Add to the list of predefined symbols
  void set_lexer_symbols(const vecteur & l,GIAC_CONTEXT){
    if (initialisation_done(contextptr))
      return;
    initialisation_done(contextptr)=true;
    const_iterateur it=l.begin(),itend=l.end();
    for (; it!=itend; ++it) {
      if (it->type!=_IDNT)
	continue;
      lock_syms_mutex();
      sym_string_tab::const_iterator i = syms().find(it->_IDNTptr->id_name),iend=syms().end();
      if (i==iend)
	syms()[it->_IDNTptr->name()] = *it;
      unlock_syms_mutex();  
    }
  }

  string replace(const string & s,char c1,char c2){
    string res;
    int l=s.size();
    res.reserve(l);
    const char * ch=s.c_str();
    for (int i=0;i<l;++i,++ch){
      res+= (*ch==c1? c2: *ch);
    }
    return res;
  }

  string replace(const string & s,char c1,const string & c2){
    string res;
    int l=s.size();
    res.reserve(l);
    const char * ch=s.c_str();
    for (int i=0;i<l;++i,++ch){
      if (*ch==c1)
        res += c2;
      else
        res += *ch;
    }
    return res;
  }

  static string remove_comment(const string & s,const string &pattern,bool rep){
    string res(s);
    for (;;){
      int pos1=res.find(pattern);
      if (pos1<0 || pos1+3>=int(res.size()))
	break;
      int pos2=res.find(pattern,pos1+3);
      if (pos2<0 || pos2+3>=int(res.size()))
	break;
      if (rep)
	res=res.substr(0,pos1)+'"'+replace(res.substr(pos1+3,pos2-pos1-3),'\n',' ')+'"'+res.substr(pos2+3,res.size()-pos2-3);
      else
	res=res.substr(0,pos1)+res.substr(pos2+3,res.size()-pos2-3);
    }
    return res;
  }

  struct int_string {
    int decal;
    std::string endbloc;
    int_string():decal(0){}
    int_string(int i,string s):decal(i),endbloc(s){}
  };

  static bool instruction_at(const string & s,int pos,int shift){
    if (pos && isalphan(s[pos-1]))
      return false;
    if (pos+shift<int(s.size()) && isalphan(s[pos+shift]))
      return false;
    return true;
  }

  void convert_python(string & cur,GIAC_CONTEXT){
    bool indexshift=array_start(contextptr); //xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38;
    if (cur[0]=='_' && (cur.size()==1 || !my_isalpha(cur[1])))
      cur[0]='@'; // python shortcut for ans(-1)
    bool instring=cur.size() && cur[0]=='"';
    int openpar=0;
    for (int pos=1;pos<int(cur.size());++pos){
      char prevch=cur[pos-1],curch=cur[pos];
      if (curch=='"' && prevch!='\\')
	instring=!instring;
      if (instring)
	continue;
      if (curch=='(')
	++openpar;
      if (curch==')')
	--openpar;
      if (curch==',' && pos<int(cur.size()-1)){
	char nextch=cur[pos+1];
	if (nextch=='}' || nextch==']' || nextch==')'){
	  cur.erase(cur.begin()+pos);
	  continue;
	}
      }
      if (curch=='}' && prevch=='{'){
	cur=cur.substr(0,pos-1)+"table()"+cur.substr(pos+1,cur.size()-pos-1);
	continue;
      }
      if (curch==':' && (prevch=='[' || prevch==',')){
	cur.insert(cur.begin()+pos,indexshift?'1':'0');
	continue;
      }
      if (curch==':' && pos<int(cur.size())-1 && cur[pos+1]!='=' && cur[pos+1]!=';'){
	int posif=cur.find("if "),curpos,cursize=int(cur.size()),count=0;
	// check is : for slicing?
	for (curpos=pos+1;curpos<cursize;++curpos){
	  if (cur[curpos]=='[')
	    ++count;
	  if (cur[curpos]==']')
	    --count;
	}
	if (count==0 && posif>=0 && posif<pos){
	  cur[pos]=')';
	  cur.insert(cur.begin()+posif+3,'(');
	  continue;
	}
      }
      if ( (curch==']' && (prevch==':' || prevch==',')) ||
	   (curch==',' && prevch==':') ){
	cur[pos-1]='.';
	cur.insert(cur.begin()+pos,'.');
	++pos;
	if (indexshift)
	  cur.insert(cur.begin()+pos,'0');
	else {
	  cur.insert(cur.begin()+pos,'1');
	  cur.insert(cur.begin()+pos,'-');
	}
	continue;
      }
      if (curch==':' && prevch==':'){
	if (pos+1<int(cur.size()) && cur[pos+1]=='-'){
	  cur.insert(cur.begin()+pos,'-');
	  cur.insert(cur.begin()+pos+1,'1');
	}
	else
	  cur.insert(cur.begin()+pos,'0');
	continue;	
      }
      if (curch=='%'){
	cur.insert(cur.begin()+pos+1,'/');
	++pos;
	continue;
      }
      if (curch=='=' && openpar==0 && prevch!='>' && prevch!='<' && prevch!='!' && prevch!=':' && prevch!=';' && prevch!='=' && prevch!='+' && prevch!='-' && prevch!='*' && prevch!='/' && prevch!='%' && (pos==int(cur.size())-1 || (cur[pos+1]!='=' && cur[pos+1]!='<' && cur[pos+1]!='>'))){
	cur.insert(cur.begin()+pos,':');
	++pos;
	continue;
      }
      if (prevch=='/' && curch=='/' && pos>1)
	cur[pos]='%';
    }
  }

  string glue_lines_backslash(const string & s){
    int ss=s.size();
    int i=s.find('\\');
    if (i<0 || i>=ss)
      return s;
    string res,line;    
    for (i=0;i<ss;++i){
      if (s[i]!='\n'){
	line += s[i];
	continue;
      }
      int ls=line.size(),j;
      for (j=ls-1;j>=0;--j){
	if (line[j]!=' ')
	  break;
      }
      if (line[j]!='\\' || (j && line[j-1]=='\\')){
	res += line+'\n';
	line ="";
      }
      else
	line=line.substr(0,j); 
    }
    return res+line;
  }

  static void python_import(string & cur,int cs,int posturtle,int poscmath,int posmath,int posnumpy,int posmatplotlib,GIAC_CONTEXT){
    if (posmatplotlib>=0 && posmatplotlib<cs){
      cur += "np:=numpy:;xlim(a,b):=gl_x=a..b:;ylim(a,b):=gl_y=a..b:;scatter:=scatterplot:;bar:=bar_plot:;text:=legend:;xlabel:=gl_x_axis_name:;ylabel:=gl_y_axis_name:;arrow:=vector:;boxplot:=moustache:;";
      posnumpy=posmatplotlib;
    }
    if (posnumpy>=0 && posnumpy<cs){
      static bool alertnum=true;
      // add python numpy shortcuts
      cur += "mat:=matrix:;arange:=range:;resize:=redim:;shape:=dim:;conjugate:=conj:;full:=matrix:;eye:=identity:;ones(n,c):=matrix(n,c,1):; astype:=convert:;float64:=float:;asarray:=array:;astype:=convert:;reshape(m,n,c):=matrix(n,c,flatten(m));";
      if (alertnum){
	alertnum=false;
	alert("mat:=matrix;arange:=range;resize:=redim;shape:=dim;conjugate:=conj;full:=matrix;eye:=idn;ones(n,c):=matrix(n,c,1);reshape(m,n,c):=matrix(n,c,flatten(m));",contextptr);
      }
      return;
    }
    if (posturtle>=0 && posturtle<cs){
      // add python turtle shortcuts
      static bool alertturtle=true;
#if defined KHICAS || defined SDL_KHICAS
      cur += "fd:=forward:;bk:=backward:; rt:=right:; lt:=left:; pos:=position:; seth:=heading:;setheading:=heading:; ";
#else
      cur += "pu:=penup:;up:=penup:; pd:=pendown:;down:=pendown:; fd:=forward:;bk:=backward:; rt:=right:; lt:=left:; pos:=position:; seth:=heading:;setheading:=heading:; reset:=efface:;";
#endif
      if (alertturtle){
	alertturtle=false;
	alert("pu:=penup;up:=penup; pd:=pendown;down:=pendown; fd:=forward;bk:=backward; rt:=right; lt:=left; pos:=position; seth:=heading;setheading:=heading; reset:=efface",contextptr);
      }
      return;
    }
    if (poscmath>=0 && poscmath<cs){
      // add python cmath shortcuts
      static bool alertcmath=true;      
      if (alertcmath){
	alertcmath=false;
	alert(gettext("Assigning phase, j, J and rect."),contextptr);
      }
      cur += "phase:=arg:;j:=i:;J:=i:;";
      posmath=poscmath;
    }
    if (posmath>=0 && posmath<cs){
      // add python math shortcuts
      static bool alertmath=true;      
      if (alertmath){
	alertmath=false;
	alert(gettext("Assigning gamma, fabs. Not supported: copysign."),contextptr);
      }
      cur += "gamma:=Gamma:;fabs:=abs:;";
    }
  }

  string replace_deuxpoints_egal(const string & s){
    string res;
    int instring=0;
    for (size_t i=0;i<s.size();++i){
      char ch=s[i];
      if (i==0 || s[i-1]!='\\'){
	if (ch=='\''){
	  if (instring==2)
	    res += ch;
	  else {
	    res +='"';
	    instring=instring?0:1;
	  }
	  continue;
	}
	if (instring){
	  if (ch=='"'){
	    if (instring==1)
	      res +="\"\"";
	    else {
	      res += ch;
	      instring=0;
	    }
	  }
	  else
	    res += ch;
	  continue;
	}
	if (ch=='"'){
	  res +='"';
	  instring=2;
	  continue;
	}
      }
      switch (ch){
      case ':':
	res +="-/-";
	break;
      case '{':
	res += "{/";
	break;
      case '}':
	res += "/}";
	break;
      default:
	res += ch;
      }
    }
    return res;
  }

  // detect Python like syntax: 
  // remove """ """ docstrings and ''' ''' comments
  // cut string in lines, remove comments at the end (search for #)
  // warning don't take care of # inside strings
  // if a line of s ends with a :
  // search for matching def/for/if/else/while
  // stores matching end keyword in a stack as a vector<[int,string]>
  // int is the number of white spaces at the start of the next line
  // def ... : -> function [ffunction]
  // for ... : -> for ... do [od]
  // while ... : -> while ... do [od]
  // if ...: -> if ... then [fi]
  // else: -> else [nothing in stack]
  // elif ...: -> elif ... then [nothing in stack]
  // try: ... except: ...
  std::string python2xcas(const std::string & s_orig,GIAC_CONTEXT){
    if (strncmp(s_orig.c_str(),"spreadsheet[",12)==0)
      return s_orig;
    if (strncmp(s_orig.c_str(),"function",8)==0 || strncmp(s_orig.c_str(),"fonction",8)==0){
      python_compat(contextptr)=0;
      return s_orig;
    }
    if (xcas_mode(contextptr)>0 && abs_calc_mode(contextptr)!=38)
      return s_orig;
    if (abs_calc_mode(contextptr)==38){
      if (s_orig.substr(0,4)=="#cas"){
        int pos=s_orig.find("#end");
        if (pos>0 && pos<s_orig.size())
          return s_orig.substr(4,pos-4);
      }
    }
    // quick check for python-like syntax: search line ending with :
    int first=0,sss=s_orig.size();
    first=s_orig.find("maple_mode");
    if (first>=0 && first<sss)
      return s_orig;
    first=s_orig.find("xcas_mode");
    if (first>=0 && first<sss)
      return s_orig;
    first=s_orig.find('\n');
    if (first<0 || first>=sss){
      first=s_orig.find('\''); // derivative or Python string delimiter?
      if (first>=0 && first<sss){
	if (first==sss-1 || s_orig[first+1]=='\'') // '' is a second derivative
	  return s_orig;
	first=s_orig.find('\'',first+1);
	if (first<0 || first>=sss)
	  return s_orig;
      }
    }
    bool pythoncompat=python_compat(contextptr);
    bool pythonmode=false;
    first=0;
    if (sss>19 && s_orig.substr(first,17)=="add_autosimplify(")
      first+=17;
    if (s_orig[first]=='/' )
      return s_orig;
    //if (sss>first+2 && s_orig[first]=='@' && s_orig[first+1]!='@') return s_orig.substr(first+1,sss-first-1);
    if (sss>first+2 && s_orig.substr(first,2)=="@@"){
      pythonmode=true;
      pythoncompat=true;
    }
    if (s_orig[first]=='#' || (s_orig[first]=='_' && !my_isalpha(s_orig[first+1])) || s_orig.substr(first,4)=="from" || s_orig.substr(first,7)=="import " || s_orig.substr(first,4)=="def "){
      pythonmode=true;
      pythoncompat=true;
    }
    if (pythoncompat){
      int pos=s_orig.find("{");
      if (pos>=0 && pos<sss)
	pythonmode=true;
      pos=s_orig.find("}");
      if (pos>=0 && pos<sss)
	pythonmode=true;
      pos=s_orig.find("//");
      if (pos>=0 && pos<sss)
	pythonmode=true;
      pos=s_orig.find(":");
      if (pos>=0 && pos<sss-1 && s_orig[pos+1]!=';')
	pythonmode=true;
    }
    for (first=0;!pythonmode && first<sss;){
      int pos=s_orig.find(":]");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      pos=s_orig.find("[:");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      pos=s_orig.find(",:");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      pos=s_orig.find(":,");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      pos=s_orig.find('#',0);
      if (pos<0 || pos>=sss){
	first=s_orig.find(':',first);
	if (first<0 || first>=sss){
	  return s_orig; // not Python like
	}
      }
      pos=s_orig.find("lambda");
      if (pos>=0 && pos<sss)
	break;
      int endl=s_orig.find('\n',first);
      if (endl<0 || endl>=sss)
	endl=sss;
      ++first;
      if (first<endl && (s_orig[first]==';' || s_orig[first]=='=')) 
	continue; // ignore :;
      // search for line finishing with : (or with # comment)
      for (;first<endl;++first){
	char ch=s_orig[first];
	if (ch!=' '){
	  if (ch=='#')
	    first=endl;
	  break;
	}
      }
      if (first==endl) 
	break;
    }
    // probably Python-like
    string res;
    res.reserve(6*s_orig.size()/5);
    res=s_orig;
    if (res.size()>18 && res.substr(0,17)=="add_autosimplify(" 
	&& res[res.size()-1]==')'
	)
      res=res.substr(17,res.size()-18);
    if (res.size()>2 && res.substr(0,2)=="@@")
      res=res.substr(2,res.size()-2);
    res=remove_comment(res,"\"\"\"",true);
    res=remove_comment(res,"'''",true);
    res=glue_lines_backslash(res);
    first=res.find('\t');
    if (first>=0 && first<res.size()){
      // replace all tabs by n spaces, n==4
      string reptab(4,' ');
      res=replace(res,'\t',reptab);
    }
    vector<int_string> stack;
    string s,cur; 
    s.reserve(res.capacity());
    if (pythoncompat) pythonmode=true;
    for (;res.size();){
      int pos=-1;
      bool cherche=true;
      for (;cherche;){
	pos=res.find('\n',pos+1);
	if (pos<0 || pos>=int(res.size()))
	  break;
	cherche=false;
	char ch=0;
	// check if we should skip to next newline, look at previous non space
	for (int pos2=0;pos2<pos;++pos2){
	  ch=res[pos2];
	  if (ch=='#')
	    break;
	}
	if (ch=='#')
	  break;
	for (int pos2=pos-1;pos2>=0;--pos2){
	  ch=res[pos2];
	  if (ch!=' ' && ch!=9 && ch!='\r'){
	    if (ch=='{' || ch=='[' || ch==',' || ch=='-' || ch=='+' ||  ch=='/'){
	      if (pos2>0 && (ch=='+' || ch=='-') && ch==res[pos2-1])
		;
	      else
		cherche=true;
	    }
	    break;
	  }
	}
	for (size_t pos2=pos+1;pos2<res.size();++pos2){
	  ch=res[pos2];
	  if (ch!=' ' && ch!=9 && ch!='\r'){
	    if (ch==']' || ch=='}' || ch==')')
	      cherche=true;
	    break;
	  }
	}
      }
      if (pos<0 || pos>=int(res.size())){
	cur=res; res="";
      }
      else {
	cur=res.substr(0,pos); // without \n
	res=res.substr(pos+1,res.size()-pos-1);
      }
      // detect comment (outside of a string) and lambda expr:expr
      bool instring=false,chkfrom=true;
      for (pos=0;pos<int(cur.size());++pos){
	char ch=cur[pos];
	if (ch==' ' || ch==char(9) || ch=='\r')
	  continue;
	if (!instring && pythoncompat && ch=='{' && (pos==0 || cur[pos-1]!='\\')){
	  // find matching }, counting : and , and ;
	  int c1=0,c2=0,c3=0,cs=int(cur.size()),p;
	  for (p=pos;p<cs;++p){
	    char ch=cur[p];
	    if (ch=='}' && cur[p-1]!='\\')
	      break;
	    if (ch==':')
	      ++c1;
	    if (ch==',')
	      ++c2;
	    if (ch==';')
	      ++c3;
	  }
	  if (p<cs 
	      //&& c1 
	      && c3==0){
	    // table initialization, replace {} by table( ) , 
	    // cur=cur.substr(0,pos)+"table("+cur.substr(pos+1,p-pos-1)+")"+cur.substr(p+1,cs-pos-1);
	    cur=cur.substr(0,pos)+"{/"+replace_deuxpoints_egal(cur.substr(pos+1,p-1-pos))+"/}"+cur.substr(p+1,cs-pos-1);
	  }
	}
	if (!instring && pythoncompat &&
	    ch=='\'' && pos<cur.size()-2 && cur[pos+1]!='\\' && (pos==0 || (cur[pos-1]!='\\' && cur[pos-1]!='\''))){ // workaround for '' string delimiters
	  static bool alertstring=true;
	  if (alertstring){
	    alert("// Python compatibility, please use \"...\" for strings",contextptr);
	    alertstring=false;
	  }
	  int p=pos,q=pos+1,beg; // skip spaces
	  for (p++;p<int(cur.size());++p)
	    if (cur[p]!=' ') 
	      break;
	  if (p!=cur.size()){
	    // find matching ' 
	    beg=q;
	    for (;p<int(cur.size());++p)
	      if (cur[p]=='\'') 
		break;
	    if (p>0 && p<int(cur.size())){
	      --p;
	      // does cur[pos+1..p-1] look like a string?
	      bool str=!my_isalpha(cur[q]) || !isalphan(cur[p]);
	      if (p && cur[p]=='.' && cur[p-1]>'9')
		str=true;
	      if (p-q>=minchar_for_quote_as_string(contextptr))
		str=true;
	      for (;!str && q<p;++q){
		char ch=cur[q];
		if (ch=='"' || ch==' ')
		  str=true;
	      }
	      if (str){ // replace delimiters with " and " inside by \"
		string rep("\"");
		for (q=beg;q<=p;++q){
		  if (cur[q]!='"')
		    rep+=cur[q];
		  else
		    rep+="\\\"";
		}
		rep += '"';
		cur=cur.substr(0,pos)+rep+cur.substr(p+2,cur.size()-(p+2));
		ch=cur[pos];
	      }
	    }
	  }
	}
	if (ch=='"' && (pos==0 || cur[pos-1]!='\\')){
	  chkfrom=false;
	  instring=!instring;
	}
	if (instring)
	  continue;
	if (ch=='#'){
	  // workaround to declare local variables
	  if (cur.size()>pos+8 && (cur.substr(pos,8)=="# local " || cur.substr(pos,7)=="#local ")){
	    cur.erase(cur.begin()+pos);
	    if (cur[pos]==' ')
	      cur.erase(cur.begin()+pos);	      
	  }
	  else
	    cur=cur.substr(0,pos);
	  pythonmode=true;
	  break;
	}
	// skip from * import *
	if (chkfrom && ch=='f' && pos+15<int(cur.size()) && cur.substr(pos,5)=="from "){
	  chkfrom=false;
	  int posi=cur.find(" import ");
	  if (posi<0 || posi>=int(cur.size()))
	    posi = cur.find(" import*");
	  if (posi>pos+5 && posi<int(cur.size())){
	    int posturtle=cur.find("turtle");
	    int poscmath=cur.find("cmath");
	    int posmath=cur.find("math");
	    int posnumpy=cur.find("numpy");
	    int posmatplotlib=cur.find("matplotlib");
	    if (posmatplotlib<0 || posmatplotlib>=cur.size())
	      posmatplotlib=cur.find("pylab");
	    int cs=int(cur.size());
	    pythonmode=true;
#if defined KHICAS || defined SDL_KHICAS
	    if (
		(posturtle<0 || posturtle>=cs) && 
		(poscmath<0 || poscmath>=cs) && 
		(posmath<0 || posmath>=cs) && 
		(posnumpy<0 || posnumpy>=cs) && 
		(posmatplotlib<0 || posmatplotlib>=cs) 
		){
	      string filename=cur.substr(pos+5,posi-pos-5)+".py";
	      // CERR << "import " << filename << endl;
	      const char * ptr=read_file(filename.c_str());
	      if (ptr)
		s += python2xcas(ptr,contextptr); // recursive call
	      cur ="";
	      // CERR << s << endl;
	      continue;
	    }
	    else
#endif
	      {
		cur=cur.substr(0,pos);
		python_import(cur,cs,posturtle,poscmath,posmath,posnumpy,posmatplotlib,contextptr);
	      }
	    break;
	  }
	}
	chkfrom=false;
	// import * as ** -> **:=*
	if (ch=='i' && pos+7<int(cur.size()) && cur.substr(pos,7)=="import "){
	  int posturtle=cur.find("turtle");
	  int poscmath=cur.find("cmath");
	  int posmath=cur.find("math");
	  int posnumpy=cur.find("numpy");
	  int posmatplotlib=cur.find("matplotlib");
	  if (posmatplotlib<0 || posmatplotlib>=cur.size())
	    posmatplotlib=cur.find("pylab");
	  int cs=int(cur.size());
	  int posi=cur.find(" as ");
	  int posp=cur.find('.');
	  if (posp>=posi || posp<0)
	    posp=posi;
	  if (posi>pos+5 && posi<int(cur.size())){
	    cur=cur.substr(posi+4,cur.size()-posi-4)+":="+cur.substr(pos+7,posp-(pos+7))+';';
	  }
	  else
	    cur=cur.substr(pos+7,cur.size()-pos-7);
	  for (int i=0;i<pos;++i)
	    cur = ' '+cur;
	  python_import(cur,cs,posturtle,poscmath,posmath,posnumpy,posmatplotlib,contextptr);
	  pythonmode=true;
	  break;	    
	}
	if (ch=='l' && pos+6<int(cur.size()) && cur.substr(pos,6)=="lambda" && instruction_at(cur,pos,6)){
	  int posdot=cur.find(':',pos);
	  if (posdot>pos+7 && posdot<int(cur.size())-1 && cur[posdot+1]!='=' && cur[posdot+1]!=';'){
	    pythonmode=true;
	    cur=cur.substr(0,pos)+"("+cur.substr(pos+6,posdot-pos-6)+")->"+cur.substr(posdot+1,cur.size()-posdot-1);
	  }
	}
	if (ch=='e' && pos+4<int(cur.size()) && cur.substr(pos,3)=="end" && instruction_at(cur,pos,3)){
	  // if next char after end is =, replace by endl
	  for (size_t tmp=pos+3;tmp<cur.size();++tmp){
	    if (cur[tmp]!=' '){
	      if (cur[tmp]=='=')
		cur.insert(cur.begin()+pos+3,'l');
	      break;
	    }
	  }
	}
      }
      if (instring){
	*logptr(contextptr) << "Warning: multi-line strings can not be converted from Python like syntax"<<'\n';
	return cur+'"';
      }
      // detect : at end of line
      for (pos=int(cur.size())-1;pos>=0;--pos){
	if (cur[pos]!=' ' && cur[pos]!=char(9) && cur[pos]!='\r')
	  break;
      }
      if (pos<0){ 
	s+='\n';  
	continue;
      }
      if (cur[pos]!=':'){ // detect oneliner and function/fonction
	int p;
	for (p=0;p<pos;++p){
	  if (cur[p]!=' ')
	    break;
	}
	if (p<pos-8 && (cur.substr(p,8)=="function" || cur.substr(p,8)=="fonction")){
	  s = s+cur+'\n';
	  continue;
	}
	bool instr=false;
	for (p=pos;p>0;--p){
	  if (instr){
	    if (cur[p]=='"' && cur[p-1]!='\\')
	      instr=false;
	    continue;
	  }
	  if (cur[p]==':' && (cur[p+1]!=';' && cur[p+1]!='='))
	    break;
	  if (cur[p]=='"' && cur[p-1]!='\\')
	    instr=true;	  
	}
	if (p==0){
	  // = or return expr if cond else alt_expr => ifte(cond,expr,alt_expr)
	  int cs=int(cur.size());
	  int elsepos=cur.find("else");
	  if (elsepos>0 && elsepos<cs){
	    int ifpos=cur.find("if");
	    if (ifpos>0 && ifpos<elsepos){
	      int retpos=cur.find("return"),endretpos=retpos+6;
	      if (retpos<0 || retpos>=cs){
		retpos=cur.find("=");
		endretpos=retpos+1;
	      }
	      if (retpos>=0 && retpos<ifpos){
		cur=cur.substr(0,endretpos)+" ifte("+cur.substr(ifpos+2,elsepos-ifpos-2)+","+cur.substr(endretpos,ifpos-endretpos)+","+cur.substr(elsepos+4,cs-elsepos-4)+")";
	      }
	    }
	  }
	}
	if (p>0){
	  int cs=int(cur.size()),q=4;
	  int progpos=cur.find("elif");;
	  if (progpos<0 || progpos>=cs){
	    progpos=cur.find("if");
	    q=2;
	  }
	  if (p>progpos && progpos>=0 && progpos<cs && instruction_at(cur,progpos,q)){
	    pythonmode=true;
#if 1
	    res = cur.substr(0,p)+":\n"+string(progpos+4,' ')+cur.substr(p+1,pos-p)+'\n'+res;
	    continue;
#else
	    cur=cur.substr(0,p)+" then "+cur.substr(p+1,pos-p);
	    convert_python(cur,contextptr);
	    // no fi if there is an else or elif
	    for (p=0;p<int(res.size());++p){
	      if (res[p]!=' ' && res[p]!=char(9) && res[p]!='\r')
		break;
	    }
	    if (p<res.size()+5 && (res.substr(p,4)=="else" || res.substr(p,4)=="elif")){
	      cur += " ";
	    }
	    else
	      cur += " fi";
	    p=0;
#endif
	  }
	  progpos=cur.find("def");
	  if (p>progpos && progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	    pythonmode=true;
	    res = cur.substr(0,p)+":\n"+string(progpos+4,' ')+cur.substr(p+1,pos-p)+'\n'+res;
	    continue;
	  }
	  progpos=cur.find("else");
	  if (p>progpos && progpos>=0 && progpos<cs && instruction_at(cur,progpos,4)){
	    pythonmode=true;
#if 1
	    res = cur.substr(0,p)+":\n"+string(progpos+4,' ')+cur.substr(p+1,pos-p)+'\n'+res;
	    continue;
#else
	    cur=cur.substr(0,p)+' '+cur.substr(p+1,pos-p)+" fi";
	    convert_python(cur,contextptr);
	    p=0;
#endif
	  }
	  progpos=cur.find("for");
	  if (p>progpos && progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	    pythonmode=true;
	    cur=cur.substr(0,p)+" do "+cur.substr(p+1,pos-p);
	    convert_python(cur,contextptr);
	    cur += " od";
	    p=0;
	  }
	  progpos=cur.find("while");
	  if (p>progpos && progpos>=0 && progpos<cs && instruction_at(cur,progpos,5)){
	    pythonmode=true;
	    cur=cur.substr(0,p)+" do "+cur.substr(p+1,pos-p);
	    convert_python(cur,contextptr);
	    cur += " od";
	    p=0;
	  }
	}
      }
      // count whitespaces, compare to stack
      int ws=0;
      int cs=cur.size();
      for (ws=0;ws<cs;++ws){
	if (cur[ws]!=' ' && cur[ws]!=char(9) && cur[ws]!='\r')
	  break;
      }
      if (cur[pos]==':'){
	// detect else or elif or except
	int progpos=cur.find("else");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,4)){
	  pythonmode=true;
	  if (stack.size()>1){ 
	    int indent=stack[stack.size()-1].decal;
	    if (ws<indent){
	      // remove last \n and add explicit endbloc delimiters from stack
	      int ss=s.size();
	      bool nl= ss && s[ss-1]=='\n';
	      if (nl)
		s=s.substr(0,ss-1);
	      while (stack.size()>1 && stack[stack.size()-1].decal>ws){
		s += ' '+stack.back().endbloc+';';
		stack.pop_back();
	      }
	      if (nl)
		s += '\n';
	    }
	  }
	  s += cur.substr(0,pos)+"\n";
	  continue;
	}
	progpos=cur.find("except");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,6)){
	  pythonmode=true;
	  if (stack.size()>1){ 
	    int indent=stack[stack.size()-1].decal;
	    if (ws<indent){
	      // remove last \n and add explicit endbloc delimiters from stack
	      int ss=s.size();
	      bool nl= ss && s[ss-1]=='\n';
	      if (nl)
		s=s.substr(0,ss-1);
	      while (stack.size()>1 && stack[stack.size()-1].decal>ws){
		s += ' '+stack.back().endbloc+';';
		stack.pop_back();
	      }
	      if (nl)
		s += '\n';
	    }
	  }
	  s += cur.substr(0,progpos)+"then\n";
	  continue;
	}
	progpos=cur.find("elif");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,4)){
	  pythonmode=true;
	  if (stack.size()>1){ 
	    int indent=stack[stack.size()-1].decal;
	    if (ws<indent){
	      // remove last \n and add explicit endbloc delimiters from stack
	      int ss=s.size();
	      bool nl= ss && s[ss-1]=='\n';
	      if (nl)
		s=s.substr(0,ss-1);
	      while (stack.size()>1 && stack[stack.size()-1].decal>ws){
		s += ' '+stack.back().endbloc+';';
		stack.pop_back();
	      }
	      if (nl)
		s += '\n';
	    }
	  }
	  cur=cur.substr(0,pos);
	  convert_python(cur,contextptr);
	  s += cur+" then\n";
	  continue;
	}
      }
      if (!stack.empty()){ 
	int indent=stack.back().decal;
	if (ws<=indent){
	  // remove last \n and add explicit endbloc delimiters from stack
	  int ss=s.size();
	  bool nl= ss && s[ss-1]=='\n';
	  if (nl)
	    s=s.substr(0,ss-1);
	  while (!stack.empty() && stack.back().decal>=ws){
	    int sb=stack.back().decal;
	    s += ' '+stack.back().endbloc+';';
	    stack.pop_back();
	    // indent must match one of the saved indent
	    if (sb!=ws && !stack.empty() && stack.back().decal<ws){
	      return "\"Bad indentation at "+cur+"\"";
	    }
	  }
	  if (nl)
	    s += '\n';
	}
      }
      if (cur[pos]==':'){
	// detect matching programming structure
	int progpos=cur.find("if");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,2)){
	  pythonmode=true;
	  cur=cur.substr(0,pos);
	  convert_python(cur,contextptr);
	  s += cur +" then\n";
	  stack.push_back(int_string(ws,"fi"));
	  continue;
	}
	progpos=cur.find("try");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	  pythonmode=true;
	  cur=cur.substr(0,progpos);
	  convert_python(cur,contextptr);
	  s += cur +"IFERR\n";
	  stack.push_back(int_string(ws,"end"));
	  continue;
	}
	progpos=cur.find("for");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	  pythonmode=true;
	  // for _ -> for x_
	  cur=cur.substr(0,pos);
	  if (progpos+5<cs && cur[progpos+3]==' ' && cur[progpos+4]=='_' && cur[progpos+5]==' '){
	    cur.insert(cur.begin()+progpos+4,'x');
	  }
	  convert_python(cur,contextptr);
	  s += cur+" do\n";
	  stack.push_back(int_string(ws,"od"));
	  continue;
	}
	progpos=cur.find("while");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,5)){
	  pythonmode=true;
	  cur=cur.substr(0,pos);
	  convert_python(cur,contextptr);
	  s += cur +" do\n";
	  stack.push_back(int_string(ws,"od"));
	  continue;
	}
	progpos=cur.find("def");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	  pythonmode=true;
	  if (abs_calc_mode(contextptr)==38) python_compat(1,contextptr); 
	  pythoncompat=true;
	  // should remove possible returned type, between -> ... and :
	  string entete=cur.substr(progpos+3,pos-progpos-3);
	  int posfleche=entete.find("->");
	  if (posfleche>0 || posfleche<entete.size())
	    entete=entete.substr(0,posfleche);
	  s += cur.substr(0,progpos)+"function"+entete+"\n";
	  stack.push_back(int_string(ws,"ffunction:")); // ; added later
	  continue;
	}
	// no match found, return s
	s = s+cur;
      }
      else {
	// normal line add ; at end
	char curpos=cur[pos];
	if (pythonmode && !res.empty() && pos>=0 && curpos!=';' && curpos!=',' && curpos!='{' && curpos!='(' && curpos!='[' && curpos!=':' && curpos!='+' && curpos!='-' && curpos!='*' && curpos!='/' && curpos!='%')
	  cur = cur +';';
	if (pythonmode)
	  convert_python(cur,contextptr);
	cur = cur +'\n';
	s = s+cur;
      }
    }
    while (!stack.empty()){
      s += ' '+stack.back().endbloc+';';
      stack.pop_back();
    }
    if (pythonmode){
      char ch;
      while (s.size()>1 && 
	     ( ((ch=s[s.size()-1])==';' && s[s.size()-2]!=':') || (ch=='\n'))
	     )
	s=s.substr(0,s.size()-1);
      // replace ;) by )
      for (int i=s.size()-1;i>=2;--i){
	if (s[i]==')' && s[i-1]=='\n' && s[i-2]==';'){
	  s.erase(s.begin()+i-2);
	  break;
	}
      }
      if (s.size()>10 && s.substr(s.size()-9,9)=="ffunction")
	s += ":;";
      else {
	int pos=s.find('\n');
	if (pos>=0 && pos<s.size() && s[s.size()-1]!=';')
	  s += ";";
      }
      if (debug_infolevel)
	*logptr(contextptr) << "Translated to Xcas as:\n" << s << '\n';
    }
    res.clear(); cur.clear();
    // CERR << s << endl;
    return string(s.begin(),s.end());
  }
  
    std::string translate_at(const char * ch){
      if (!strcmp(ch,"∡"))
	return "polar_complex";
      if (!strcmp(ch,"."))
	return "struct_dot";
      if (!strcmp(ch,"LINEAR?"))
	return "IS_LINEAR";
      if (!strcmp(ch,"ΔLIST"))
	return "DELTALIST";
      if (!strcmp(ch,"ΠLIST"))
	return "PILIST";
      if (!strcmp(ch,"ΣLIST"))
	return "SIGMALIST";
      if (!strcmp(ch,"∫"))
	return "HPINT";
      if (!strcmp(ch,"∂"))
	return "HPDIFF";
      if (!strcmp(ch,"Σ"))
	return "HPSUM";
      if (!strcmp(ch,"∑"))
	return "HPSUM";
      string res;
      for (;*ch;++ch){
        if (*ch=='%')
          res +="PERCENT";
        else
          res += *ch;
      }
      return res;
    }
    
    bool builtin_lexer_functions_sorted = false;

    map_charptr_gen & lexer_functions(){
      static map_charptr_gen * ans=0;
      if (!ans){
	ans = new map_charptr_gen;
	doing_insmod=false;
	builtin_lexer_functions_sorted=false;
      }
      return * ans;
    }


#ifdef STATIC_BUILTIN_LEXER_FUNCTIONS
    
    const charptr_gen_unary builtin_lexer_functions[] ={
#if defined(GIAC_HAS_STO_38) && defined(CAS38_DISABLED)
#include "static_lexer_38.h"
#else
#if defined KHICAS || defined NSPIRE_NEWLIB
#include "static_lexer_numworks.h"
#else
#include "static_lexer.h"
#endif // KHICAS
#endif
    };

    const unsigned builtin_lexer_functions_number=sizeof(builtin_lexer_functions)/sizeof(charptr_gen_unary);
    // return true/false to tell if s is recognized. return the appropriate gen if true
#ifdef NSPIRE
    vector<size_t> * builtin_lexer_functions_(){
      static vector<size_t> * res=0;
      if (res) return res;
      res = new vector<size_t>;
      res->reserve(builtin_lexer_functions_number+1);
#include "static_lexer_at.h"
      return res;
    }
#else
    // Array added because GH compiler stores builtin_lexer_functions in RAM
#if defined KHICAS || defined NSPIRE_NEWLIB
    const unary_function_ptr * const * const builtin_lexer_functions_[]={
#include "static_lexer__numworks.h"
    };
#else
    const size_t builtin_lexer_functions_[]={
#if defined(GIAC_HAS_STO_38) && defined(CAS38_DISABLED)
#include "static_lexer_38_.h"
#else
#include "static_lexer_.h"
#endif
    };
#endif // KHICAS
#endif // STATIC_BUILTIN

#ifdef SMARTPTR64
    charptr_gen * builtin_lexer_functions64(){
      static charptr_gen * ans=0;
      if (!ans){
	ans = new charptr_gen[builtin_lexer_functions_number];
	for (unsigned i=0;i<builtin_lexer_functions_number;i++){
	  charptr_gen tmp; tmp.first=builtin_lexer_functions[i].s; tmp.second=builtin_lexer_functions[i]._FUNC_;
	  tmp.second.subtype=builtin_lexer_functions[i].subtype;
	  ans[i]=tmp;
	}
      }
      return ans;
    }

    charptr_gen * builtin_lexer_functions_begin(){
      return (charptr_gen *) builtin_lexer_functions64();
    }
#else
    charptr_gen * builtin_lexer_functions_begin(){
      return (charptr_gen *) builtin_lexer_functions;
    }
#endif // SMARTPTR64

    charptr_gen * builtin_lexer_functions_end(){
      return builtin_lexer_functions_begin()+builtin_lexer_functions_number;
    }

#else
    unsigned builtin_lexer_functions_number;
    charptr_gen * builtin_lexer_functions(){
      static charptr_gen * ans=0;
      if (!ans){
	ans = new charptr_gen[1999];
	builtin_lexer_functions_number=0;
      }
      return ans;
    }

    charptr_gen * builtin_lexer_functions_begin(){
      return builtin_lexer_functions();
    }

    charptr_gen * builtin_lexer_functions_end(){
      return builtin_lexer_functions()+builtin_lexer_functions_number;
    }

    const size_t * const builtin_lexer_functions_=0;

#endif

#endif // NSPIRE

  gen make_symbolic(const gen & op,const gen & args){
    return symbolic(*op._FUNCptr,args);
  }

  // optional, call it just before exiting
  int release_globals(){
#if !defined VISUALC && !defined KHICAS
    delete normal_sin_pi_12_ptr_();
    delete normal_cos_pi_12_ptr_();
#endif
#ifndef STATIC_BUILTIN_LEXER_FUNCTIONS
    if (debug_infolevel)
      CERR << "releasing " << builtin_lexer_functions_number << " functions" << '\n';
    for (int i=0;i<builtin_lexer_functions_number;++i){
#ifdef SMARTPTR64
      if (debug_infolevel)
	CERR << builtin_lexer_functions_begin()[i].first << '\n'; 
      builtin_lexer_functions_begin()[i].second=0;
      //delete (ref_unary_function_ptr *) (* ((ulonglong * ) &builtin_lexer_functions_begin()[i].second) >> 16);
#endif
    }
#endif
    delete &registered_lexer_functions();
    delete &lexer_functions();
    delete &library_functions();
    delete &lexer_translator();
    delete &back_lexer_localization_map();
    delete &lexer_localization_map();
    delete &lexer_localization_vector();
    delete &syms();
    delete &unit_conversion_map();
    delete &xcasrc();
    //delete &usual_units();
    if (vector_aide_ptr()) delete vector_aide_ptr();
    delete &symbolic_rootof_list();
    delete &proot_list();
    delete &galoisconj_list();
    delete &_autoname_();
    delete &_lastprog_name_();
    return 0;
  }

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC

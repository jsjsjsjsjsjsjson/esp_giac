/* -*- mode:C++ ; compile-command: "g++ -I.. -I../include -I.. -g -c -fno-strict-aliasing -DGIAC_GENERIC_CONSTANTS -DHAVE_CONFIG_H -DIN_GIAC -Wall cocoa.cc" -*- */
// Use GIAC_DEBUG_TDEG_T64 to debug potential memory errors with large number of variables
// Thanks to Zoltan Kovacs for motivating this work, in order to improve geogebra theorem proving
// Special thanks to Anna M. Bigatti from CoCoA team for insightfull discussions on how to choose an order for elimination. This file name is kept to remind that the first versions of giac were using CoCoA for Groebner basis computations, before a standalone implementation.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// vector class by Agner Fog https://github.com/vectorclass
// this might be faster for CPU with AVX512DQ instruction set
// (fast multiplication of Vec4q)
#if defined HAVE_VCL2_VECTORCLASS_H 
// https://github.com/vectorclass, compile with -mavx2 -mfma 
#include <vcl2/vectorclass.h>
#ifdef __AVX2__
#define CPU_SIMD
#endif
#endif

#include "modint.h"

#include "giacPCH.h"
//#define EMCC


// if GIAC_SHORTSHIFTTYPE is defined, sparse matrix is using shift index
// coded on 2 bytes 
#define GIAC_SHORTSHIFTTYPE 16
// MAXNTHREADS is used for arrays with threads information
#define MAXNTHREADS 64

// #define GBASIS_4PRIMES to run 4 primes reduction simultaneously
#if defined __AVX2__ && !defined EMCC && !defined EMCC2 && defined GIAC_SHORTSHIFTTYPE && GIAC_SHORTSHIFTTYPE==16
#define GBASIS_4PRIMES
#endif

#ifdef WORDS_BIGENDIAN // autoconf macro defines this (thanks to Julien Puydt for pointing this and checking for s390x architecture)
#define BIGENDIAN
#endif


#ifndef WIN32
#define COCOA9950
#endif

#ifdef HAVE_LIBPTHREAD
#endif

#if 0 // works faster with AVX2 only
#include <vcl2/vectorclass.h>
#define CPU_SIMD // should be configured in config.h
// add -std=c++17 to the compiler options
#endif

#ifdef BF2GMP_H
#define USE_GMP_REPLACEMENTS
#endif

#if defined(USE_GMP_REPLACEMENTS) || defined(GIAC_VECTOR)
#undef HAVE_LIBCOCOA
#endif
#ifdef HAVE_LIBCOCOA
#ifdef COCOA9950
#include <CoCoA/RingZZ.H>
#include <CoCoA/RingQQ.H>
#else
#include <CoCoA/ZZ.H>
#include <CoCoA/RingQ.H>
#endif
#include <CoCoA/FractionField.H>
#include <CoCoA/GlobalManager.H>
#include <CoCoA/SparsePolyRing.H>
#include <CoCoA/TmpF5.H>
//#include <CoCoA/io.H>
#include <CoCoA/VectorOps.H>
#include <CoCoA/obsolescent.H>
#include <CoCoA/SparsePolyOps-RingElem.H>
#include <CoCoA/SparsePolyIter.H>
#include <CoCoA/BigInt.H>
//
#include <CoCoA/symbol.H>
#include "TmpFGLM.H"
#endif
/*  
 *  Copyright (C) 2007,2014 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
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
using namespace std;
#ifdef ATOMIC // attempt to make tdeg_t64 ref counter threadsafe, but fails
#include <atomic>
#endif

#include <iostream>
//#include <fstream>
#if !defined FXCG && !defined KHICAS && !defined SDL_KHICAS
#include <iomanip>
#endif
#include "cocoa.h"
#include "gausspol.h"
#include "identificateur.h"
#include "giacintl.h"
#include "index.h"
#include "modpoly.h"
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#if defined(USE_GMP_REPLACEMENTS) || defined(GIAC_VECTOR)
#undef HAVE_LIBCOCOA
#endif

#if defined VISUALC && defined x86_64 
#undef x86_64
#endif

inline mod4int modulo(mpz_t & z,const mod4int & m){
  mod4int res={giac::modulo(z,m.tab[0]),giac::modulo(z,m.tab[1]),giac::modulo(z,m.tab[2]),giac::modulo(z,m.tab[3])};
  return res;
}
inline mod4int invmod(const mod4int &a,const mod4int & p){
  mod4int res={giac::invmod(a.tab[0],p.tab[0]),
    giac::invmod(a.tab[1],p.tab[1]),
    giac::invmod(a.tab[2],p.tab[2]),
    giac::invmod(a.tab[3],p.tab[3])};
  return res;
}
std::ostream & operator << (std::ostream & os,const mod4int & a){
  return os<< "(" << a.tab[0] << ","<<a.tab[1] << "," << a.tab[2] << "," << a.tab[3] << ")";
}

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  longlong memory_usage(){
#if defined HAVE_SYS_RESOURCE_H && !defined NSPIRE && !defined NSPIRE_NEWLIB
    struct rusage r_usage;
    getrusage(RUSAGE_SELF,&r_usage);
#ifdef __APPLE__
    return r_usage.ru_maxrss;
#else
    return r_usage.ru_maxrss*1000;
#endif
#endif
    return -1;
  }

  //  vecteur trim(const vecteur & p,environment * env);

#ifdef HAVE_LIBCOCOA
  struct order_vectpoly {
    int order;
    vectpoly v;
  };

  struct Qx_I {
    CoCoA::SparsePolyRing * Qxptr;
    CoCoA::ideal * idealptr;
    CoCoA::PPOrdering * cocoa_order;
    Qx_I(): Qxptr(0), idealptr(0),cocoa_order(0){}
  };

  static bool operator < (const order_vectpoly & ov1,const order_vectpoly & ov2){
    if (ov1.order!=ov2.order)
      return ov1.order<ov2.order;
    unsigned ov1s=ov1.v.size(),ov2s=ov2.v.size();
    if (ov1s!=ov2s)
      return ov1s<ov2s;
    for (unsigned i=0;i<ov1s;++i){
      if (ov1.v[i].dim!=ov2.v[i].dim)
	return ov1.v[i].dim<ov2.v[i].dim;
      polynome p = ov1.v[i]-ov2.v[i];
      if (p.coord.empty())
	continue;
      return p.coord.front().value.islesscomplexthan(0);
    }
    return false;
  }
  
  static CoCoA::GlobalManager CoCoAFoundations;

  // cache here a list of already known ideals
  static std::map<order_vectpoly,Qx_I> cocoa_idealptr_map;

#ifdef COCOA9950
  static CoCoA::BigInt gen2ZZ(const gen & g){
    switch (g.type){
    case _INT_:
      return CoCoA::BigInt(g.val);
    case _ZINT:
#ifdef COCOA9950
      return CoCoA::BigIntFromMPZ(*g._ZINTptr);
      //return CoCoA::BigInt(*g._ZINTptr);
#else
      return CoCoA::BigInt(CoCoA::CopyFromMPZ,*g._ZINTptr);
#endif
    default:
      setsizeerr(gettext("Invalid giac gen -> CoCoA ZZ conversion")+g.print());
      return CoCoA::BigInt(0);
    }
  }

  static gen ZZ2gen(const CoCoA::RingElem & z){
    CoCoA::BigInt n,d;
    if (CoCoA::IsInteger(n, z))
      return gen(CoCoA::mpzref(n));
    CoCoA::RingElem znum=CoCoA::num(z),zden=CoCoA::den(z);
    if (CoCoA::IsInteger(n, znum) && CoCoA::IsInteger(d, zden))
      return gen(CoCoA::mpzref(n))/gen(CoCoA::mpzref(d));
    setsizeerr(gettext("Unable to convert CoCoA data"));
    return undef;
  }
#else
  static CoCoA::ZZ gen2ZZ(const gen & g){
    switch (g.type){
    case _INT_:
      return CoCoA::ZZ(g.val);
    case _ZINT:
      return CoCoA::ZZ(CoCoA::CopyFromMPZ,*g._ZINTptr);
    default:
      setsizeerr(gettext("Invalid giac gen -> CoCoA ZZ conversion")+g.print());
      return CoCoA::ZZ(0);
    }
  }

  static gen ZZ2gen(const CoCoA::RingElem & z){
    CoCoA::ZZ n,d;
    if (CoCoA::IsInteger(n, z))
      return gen(CoCoA::mpzref(n));
    CoCoA::RingElem znum=CoCoA::num(z),zden=CoCoA::den(z);
    if (CoCoA::IsInteger(n, znum) && CoCoA::IsInteger(d, zden))
      return gen(CoCoA::mpzref(n))/gen(CoCoA::mpzref(d));
    setsizeerr(gettext("Unable to convert CoCoA data"));
    return undef;
  }
#endif

  static CoCoA::RingElem polynome2ringelem(const polynome & p,const std::vector<CoCoA::RingElem> & x){
    if (unsigned(p.dim)>x.size())
      setdimerr();
    CoCoA::RingElem res(x[0]-x[0]); // how do you construct 0 in CoCoA?
    vector<monomial<gen> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    for (;it!=itend;++it){
      CoCoA::RingElem tmp(gen2ZZ(it->value)*power(x[0],0));
      index_t::const_iterator jt=it->index.begin(),jtend=it->index.end();
      for (int i=0;jt!=jtend;++jt,++i)
	tmp *= power(x[i],*jt);
      res += tmp;
    }
    return res;
  }

  static polynome ringelem2polynome(const CoCoA::RingElem & f,const gen & order){
    CoCoA::SparsePolyIter it=CoCoA::BeginIter(f);
    unsigned dim=CoCoA::IsEnded(it)?0:CoCoA::NumIndets(CoCoA::owner(CoCoA::PP(it)));
    polynome res(dim);
    vector<long> expv;
    index_t index(dim);
    for (;!CoCoA::IsEnded(it);++it){
      const CoCoA::RingElem & z=CoCoA::coeff(it);
      gen coeff=ZZ2gen(z);
      const CoCoA::PPMonoidElem & pp=CoCoA::PP(it);
      CoCoA::exponents(expv,pp);
      for (unsigned i=0;i<dim;++i)
	index[i]=expv[i];
      res.coord.push_back(monomial<gen>(coeff,index));
    }
    change_monomial_order(res,order); // res.tsort();
    return res;
  }

  static void vector_polynome2vector_ringelem(const vectpoly & v,const CoCoA::SparsePolyRing & Qx,vector<CoCoA::RingElem> & g){
    const vector<CoCoA::RingElem> & x = CoCoA::indets(Qx);
    g.reserve(v.size());
    vectpoly::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      g.push_back(polynome2ringelem(*it,x));
    }
  }
#if 0
  static vector<CoCoA::RingElem> vector_polynome2vector_ringelem(const vectpoly & v,const CoCoA::SparsePolyRing & Qx){
    vector<CoCoA::RingElem> g;
    vector_polynome2vector_ringelem(v,Qx,g);
    return g;
  }
  static vectpoly vector_ringelem2vector_polynome(const vector<CoCoA::RingElem> & g,const gen & order){
    vectpoly res;
    vector_ringelem2vector_polynome(g,res,order);
    return res;
  }
#endif
  static void vector_ringelem2vector_polynome(const vector<CoCoA::RingElem> & g, vectpoly & res,const gen & order){
    vector<CoCoA::RingElem>::const_iterator it=g.begin(),itend=g.end();
    res.reserve(itend-it);
    for (;it!=itend;++it)
      res.push_back(ringelem2polynome(*it,order));
    sort(res.begin(),res.end(),tensor_is_strictly_greater<gen>);
    reverse(res.begin(),res.end());    
  }

  static Qx_I get_or_make_idealptr(const vectpoly & v,const gen & order){
    if (order.type!=_INT_ || v.empty())
      settypeerr();
    order_vectpoly ov;
    ov.v=v;
    ov.order=order.val;
    std::map<order_vectpoly,Qx_I>::const_iterator it=cocoa_idealptr_map.find(ov),itend=cocoa_idealptr_map.end();
    if (it!=itend)
      return it->second;
    int d=v[0].dim;
    Qx_I qx_i;
    if (order.type==_INT_ && order.val!=0){
      switch (order.val){
      case _PLEX_ORDER:
	qx_i.cocoa_order = new CoCoA::PPOrdering(CoCoA::NewLexOrdering(d));
	break;
      case _TDEG_ORDER:
	qx_i.cocoa_order = new CoCoA::PPOrdering(CoCoA::NewStdDegLexOrdering(d));
	break;
      default:
	qx_i.cocoa_order = new CoCoA::PPOrdering(CoCoA::NewStdDegRevLexOrdering(d));
      }
      qx_i.Qxptr = new CoCoA::SparsePolyRing(CoCoA::NewPolyRing(
#ifdef COCOA9950
								CoCoA::RingQQ(), 
#else
								CoCoA::RingQ(), 
#endif
								CoCoA::SymbolRange("x",0,d-1),*qx_i.cocoa_order));
    }
    else
      qx_i.Qxptr = new CoCoA::SparsePolyRing(CoCoA::NewPolyRing(
#ifdef COCOA9950
								CoCoA::RingQQ(), 
#else
								CoCoA::RingQ(), 
#endif
								CoCoA::SymbolRange("x",0,d-1)));
    vector<CoCoA::RingElem> g;
    vector_polynome2vector_ringelem(v,*qx_i.Qxptr,g);
    qx_i.idealptr=new CoCoA::ideal(*qx_i.Qxptr,g);
    cocoa_idealptr_map[ov]=qx_i;
    // if (cocoa_order)
    //  delete cocoa_order;
    return qx_i;
  }

  // add a dimension so that p is homogeneous of degree d
  static void homogeneize(polynome & p,int deg){
    vector<monomial<gen> >::iterator it=p.coord.begin(),itend=p.coord.end();
    int n;
    for (;it!=itend;++it){
      index_t i=it->index.iref();
      n=total_degree(i);
      i.push_back(deg-n);
      it->index=i;
    }
    ++p.dim;
  }

  static void homogeneize(vectpoly & v){
    vectpoly::iterator it=v.begin(),itend=v.end();
    int d=0;
    for (;it!=itend;++it){
      d=giacmax(d,total_degree(*it));
    }
    for (it=v.begin();it!=itend;++it){
      homogeneize(*it,d);
    }
  }

  static void unhomogeneize(polynome & p){
    vector<monomial<gen> >::iterator it=p.coord.begin(),itend=p.coord.end();
    for (;it!=itend;++it){
      index_t i=it->index.iref();
      i.pop_back();
      it->index=i;
    }
    --p.dim;
  }

  static void unhomogeneize(vectpoly & v){
    vectpoly::iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      unhomogeneize(*it);
    }
  }

  bool f5(vectpoly & v,const gen & order){
    homogeneize(v);
    CoCoA::SparsePolyRing Qx = CoCoA::NewPolyRing(
#ifdef COCOA9950
								CoCoA::RingQQ(), 
#else
								CoCoA::RingQ(), 
#endif
						  CoCoA::SymbolRange("x",0,v[0].dim-1));
    vector<CoCoA::RingElem> g;
    vector_polynome2vector_ringelem(v,Qx,g);
    CoCoA::ideal I(Qx,g);
    vector<CoCoA::RingElem> gb;
    CoCoA::F5(gb,I);
    CoCoA::operator<<(cout,gb);
    cout << '\n';
    v.clear();
    vector_ringelem2vector_polynome(gb,v,order);
    unhomogeneize(v);
    return true;
  }

  // order may be 0 (use cocoa default and convert to lex for 0-dim ideals)
  // or _PLEX_ORDER (lexicographic) or _TDEG_ORDER (total degree) 
  // or _REVLEX_ORDER (total degree then reverse of lexicographic)
  bool cocoa_gbasis(vectpoly & v,const gen & order){
    Qx_I qx_i = get_or_make_idealptr(v,order);
    int d=v[0].dim;
    vector<CoCoA::RingElem> gb=TidyGens(*qx_i.idealptr);
    // CoCoA::operator<<(cout,gb);   
    // cout << '\n';
    // 0-dim ideals convert to lexicographic order using CoCoA FGLM routine
    // otherwise leaves revlex order
    vector<CoCoA::RingElem> NewGBasis;
    if (order.type==_INT_ && order.val!=0)
      NewGBasis=gb;
    else {
      CoCoA::PPOrdering NewOrdering = CoCoA::NewLexOrdering(d);
      try {
	CoCoADortmund::FGLMBasisConversion(NewGBasis, gb, NewOrdering);
      } catch (...){
	v.clear();
	vector_ringelem2vector_polynome(gb,v,order);
	return false;
      }
      // CoCoA::operator<<(cout,NewGBasis);   
      // cout << '\n';
    }
    v.clear();
    vector_ringelem2vector_polynome(NewGBasis,v,order);
    // reverse(v.begin(),v.end());
    // unhomogeneize(v);
    return true;
  }

  vecteur cocoa_in_ideal(const vectpoly & r,const vectpoly & v,const gen & order){
    Qx_I qx_i = get_or_make_idealptr(v,order);
    vector<CoCoA::RingElem> cocoa_r;
    vector_polynome2vector_ringelem(r,*qx_i.Qxptr,cocoa_r);
    int s=cocoa_r.size();
    gen tmp(-1);
    tmp.subtype=_INT_BOOLEAN;
    vecteur res(s,tmp);
    for (int i=0;i<s;++i)
      res[i].val=CoCoA::IsElem(cocoa_r[i],*qx_i.idealptr);
    return res;
  }

  bool cocoa_greduce(const vectpoly & r,const vectpoly & v,const gen & order,vectpoly & res){
    Qx_I qx_i = get_or_make_idealptr(v,order);
    vector<CoCoA::RingElem> cocoa_r;
    vector_polynome2vector_ringelem(r,*qx_i.Qxptr,cocoa_r);
    int s=cocoa_r.size();
    polynome tmp;
    for (int i=0;i<s;++i){
      CoCoA::RingElem tmpc=CoCoA::NF(cocoa_r[i],*qx_i.idealptr);
      tmp=ringelem2polynome(tmpc,order);
      res.push_back(tmp);
    }
    return true;
  }

#else // HAVE_LIBCOCOA

  bool f5(vectpoly & v,const gen & ordre){
    return false;
  }

  bool cocoa_gbasis(vectpoly & v,const gen & ordre){
    return false;
  }

  vecteur cocoa_in_ideal(const vectpoly & r,const vectpoly & v,const gen & ordre){
    return vecteur(r.size(),-1);
  }

  bool cocoa_greduce(const vectpoly & r,const vectpoly & v,const gen & order,vectpoly & res){
    return false;
  }

#endif  // HAVE_LIBCOCOA

  struct paire {
    unsigned first;
    unsigned second;
    bool live; // set to false to defer the pair for reduction at end
    paire(unsigned f,unsigned s):first(f),second(s),live(true){}
    paire(const paire & p):first(p.first),second(p.second),live(p.live) {}
    paire():first(-1),second(-1) {}
  };

  ostream & operator << (ostream & os,const paire & p){
    return os << "<" << p.first << "," << p.second << ">";
  }

  inline bool operator == (const paire & a,const paire &b){
    return a.first==b.first && a.second==b.second;
  }
  

  inline bool operator < (const paire & a,const paire &b){
    return a.first!=b.first?a.first<b.first:a.second<b.second;
  }

  inline bool tripair (const pair<int,double> & a,const pair<int,double> &b){
    return a.second<b.second;
  }
  
#if !defined CAS38_DISABLED && !defined FXCG && !defined KHICAS && !defined SDL_KHICAS
  //#define GBASIS_SELECT_TOTAL_DEGREE
#if GROEBNER_VARS!=15 && !defined BIGENDIAN // double revlex ordering is not compatible with indices swapping for tdeg_t64
#define GBASIS_SWAP 
#endif
  // minimal numbers of pair to reduce simultaneously with f4buchberger
#if 1 // def __APPLE__
#define GBASISF4_BUCHBERGER 0 // disabled
#else
#define GBASISF4_BUCHBERGER 1 
// insure same pairs, if >1 the pairs are reduced one by one (more iteration)
#endif

#define GBASIS_POSTF4BUCHBERGER 0 // 0 means final simplification at the end, 1 at each loop

  // #define GIAC_GBASIS_REDUCTOR_MAXSIZE 10 // max size for keeping a reductor even if it should be removed from gbasis
  
  //#define GIAC_GBASIS_DELAYPAIRS

  void swap_indices(short * tab){
    swap(tab[1],tab[3]);
    swap(tab[4],tab[7]);
    swap(tab[5],tab[6]);
    swap(tab[8],tab[11]);
    swap(tab[9],tab[10]);
#if GROEBNER_VARS>11
    swap(tab[12],tab[15]);
    swap(tab[13],tab[14]);
#endif
  }

  template<class T>
  void swap_indices14(T * tab){
    swap(tab[2],tab[7]);
    swap(tab[3],tab[6]);
    swap(tab[4],tab[5]);
    swap(tab[8],tab[15]);
    swap(tab[9],tab[14]);
    swap(tab[10],tab[13]);
    swap(tab[11],tab[12]);
  }

  void swap_indices11(short * tab){
    swap(tab[1],tab[3]);
    swap(tab[4],tab[7]);
    swap(tab[5],tab[6]);
    swap(tab[8],tab[11]);
    swap(tab[9],tab[10]);
  }

  void swap_indices15_revlex(short * tab){
    swap(tab[1],tab[3]);
    swap(tab[4],tab[7]);
    swap(tab[5],tab[6]);
    swap(tab[8],tab[11]);
    swap(tab[9],tab[10]);
    swap(tab[12],tab[15]);
    swap(tab[13],tab[14]);
  }

  void swap_indices15_3(short * tab){
    swap(tab[1],tab[3]);
    swap(tab[5],tab[7]);
    swap(tab[8],tab[11]);
    swap(tab[9],tab[10]);
    swap(tab[12],tab[15]);
    swap(tab[13],tab[14]);
  }

  void swap_indices15_7(short * tab){
    swap(tab[1],tab[3]);
    swap(tab[4],tab[7]);
    swap(tab[5],tab[6]);
    swap(tab[9],tab[11]);
    swap(tab[12],tab[15]);
    swap(tab[13],tab[14]);
  }

  void swap_indices15_11(short * tab){
    swap(tab[1],tab[3]);
    swap(tab[4],tab[7]);
    swap(tab[5],tab[6]);
    swap(tab[8],tab[11]);
    swap(tab[9],tab[10]);
    swap(tab[13],tab[15]);
  }

  void swap_indices15(short * tab,int o){
    if (o==_REVLEX_ORDER){
      swap_indices15_revlex(tab);
      return;
    }
    if (o==_3VAR_ORDER){
      swap_indices15_3(tab);
      return;
    }
    if (o==_7VAR_ORDER){
      swap_indices15_7(tab);
      return;
    }
    if (o==_11VAR_ORDER){
      swap_indices15_11(tab);
      return;
    }
  }

  // #define GIAC_CHARDEGTYPE should be in solve.h
#if defined BIGENDIAN && defined GIAC_CHARDEGTYPE
#undef GIAC_CHARDEGTYPE
#endif

#ifdef GIAC_CHARDEGTYPE
  typedef unsigned char degtype; // type for degree for large number of variables
  #define degratio 8
  #define degratiom1 7
#else
  typedef short degtype; // type for degree for large number of variables
  #define degratio 4
  #define degratiom1 3
#endif

#ifndef GIAC_64VARS
#define GBASIS_NO_OUTPUT
#endif 

#define GIAC_RDEG
  // #define GIAC_HASH  
#if defined GIAC_HASH && defined HASH_MAP_NAMESPACE
#define GIAC_RHASH
#endif
  
#ifdef GIAC_RHASH
  class tdeg_t64;
  class tdeg_t64_hash_function_object {
  public:
    size_t operator () (const tdeg_t64 & ) const; // defined at the end of this file
    tdeg_t64_hash_function_object() {};
  };

  typedef HASH_MAP_NAMESPACE::hash_map< tdeg_t64,int,tdeg_t64_hash_function_object > tdeg_t64_hash_t ;
#endif
  
#ifndef VISUALC
  #define GIAC_ELIM
#endif
  short hash64tab[]={1933,1949,1951,1973,1979,1987,1993,1997,1999,2003,2011,2017,2027,2029,2039,2053,2063,2069,2081,2083,2087,2089,2099,2111,2113,2129,2131,2137,2141,2143,2153,2161,2179,2203,2207,2213,2221,2237,2239,2243,2251,2267,2269,2273,2281,2287,2293,2297,2309,2311,2333,2339,2341,2347,2351,2357,2371,2377,2381,2383,2389,2393,2399,2411};

  // storing indices in reverse order to have tdeg_t_greater access them
  // in increasing order seems slower (cocoa.cc.64)
  struct tdeg_t64 {
    bool vars64() const {return true;}
#ifdef GIAC_RHASH
    int hash_index(void * ptr_) const {
      if (!ptr_) return -1;
      tdeg_t64_hash_t & h=*(tdeg_t64_hash_t *) ptr_;
      tdeg_t64_hash_t::const_iterator it=h.find(*this),itend=h.end();
      if (it==itend)
	return -1;
      return it->second;
    }
    bool add_to_hash(void *ptr_,int no) const {
      if (!ptr_) return false;
      tdeg_t64_hash_t & h=*(tdeg_t64_hash_t *) ptr_;
      h[*this]=no;
      return true;
    }
#else
  int hash_index(void * ptr_) const { return -1; }
  bool add_to_hash(void *ptr_,int no) const { return false; }  
#endif
    void dbgprint() const;
    // data
#ifdef GIAC_64VARS
    union {
      short tab[GROEBNER_VARS+1];
      struct {
	short tdeg; // actually it's twice the total degree+1
	short tdeg2;
	order_t order_;
	longlong * ui;
#ifdef GIAC_HASH
	longlong hash;
#endif
#ifdef GIAC_ELIM
	ulonglong elim; // used for elimination order (modifies revlex/revlex)
#endif
      };
    };
    //int front() const { if (tdeg % 2) return (*(ui+1)) & 0xffff; else return order_.o==_PLEX_ORDER?tab[0]:tab[1];}
    tdeg_t64(const tdeg_t64 & a){
      if (a.tab[0]%2){
	tdeg=a.tdeg;
	tdeg2=a.tdeg2;
	order_=a.order_;
	ui=a.ui;
#ifdef GIAC_HASH
	hash=a.hash;
#endif
#ifdef GIAC_ELIM
	elim=a.elim;
#endif
	++(*ui);
      }
      else {
	longlong * ptr = (longlong *) tab;
	longlong * aptr = (longlong *) a.tab;
	ptr[0]=aptr[0];
	ptr[1]=aptr[1];
	ptr[2]=aptr[2];
	ptr[3]=aptr[3];
      }
    }
#ifdef GIAC_ELIM
    void compute_elim(longlong * ptr,longlong * ptrend){
      elim=0;
      if (order_.o==_PLEX_ORDER) // don't use elim for plex
	return;
      if (tdeg>31){
	elim=0x1fffffffffffffffULL;
	return;
      }
      bool tdegcare=false;
      if (ptr<ptrend-3)
	ptr=ptrend-3;
      for (--ptrend;ptr<=ptrend;--ptrend){
	longlong x=*ptrend;
	elim <<= 20;
	elim += (x&0xffff)+(((x>>16)&0xffff)<<5)+(((x>>32)&0xffff)<<10)+(((x>>48)&0xffff)<<15);
      }
    }
#endif
    void compute_degs(){
      if (tab[0]%2){
	longlong * ptr=ui+1;
	tdeg=0;
	int firstblock=order_.o;
	if (firstblock!=_3VAR_ORDER && firstblock<_7VAR_ORDER)
	  firstblock=order_.dim;
	longlong * ptrend=ui+1+(firstblock+degratiom1)/degratio;
#ifdef GIAC_HASH
	hash=0;
#endif
#ifdef GIAC_ELIM
	compute_elim(ptr,ptrend);
#endif
	int i=0;
	for (;ptr!=ptrend;i+=4,++ptr){
	  longlong x=*ptr;
#ifdef GIAC_CHARDEGTYPE
	  tdeg += ((x+(x>>8)+(x>>16)+(x>>24)+(x>>32)+(x>>40)+(x>>48)+(x>>56))&0xff);
#else
	  tdeg += ((x+(x>>16)+(x>>32)+(x>>48))&0xffff);
#endif
#ifdef GIAC_HASH
	  hash += (x&0xffff)*hash64tab[i]+((x>>16)&0xffff)*hash64tab[i+1]+((x>>32)&0xffff)*hash64tab[i+2]+(x>>48)*hash64tab[i+3];
#endif
	}
#ifdef GIAC_ELIM
	if (tdeg>=16)
	  elim=0x1fffffffffffffffULL;
#endif
	tdeg=2*tdeg+1;
	tdeg2=0;
	ptrend=ui+1+(order_.dim+degratiom1)/degratio;
	for (;ptr!=ptrend;i+=4,++ptr){
	  longlong x=*ptr;
#ifdef GIAC_CHARDEGTYPE
	  tdeg2 += ((x+(x>>8)+(x>>16)+(x>>24)+(x>>32)+(x>>40)+(x>>48)+(x>>56))&0xff);
#else
	  tdeg2 += ((x+(x>>16)+(x>>32)+(x>>48))&0xffff);
#endif
#ifdef GIAC_HASH
	  hash += (x&0xffff)*hash64tab[i]+((x>>16)&0xffff)*hash64tab[i+1]+((x>>32)&0xfff)*hash64tab[i+2]+(x>>48)*hash64tab[i+3];
#endif
	}
      }
    }
    ~tdeg_t64(){
      if ((tab[0]%2) && ui){
#ifdef ATOMIC
	if (atomic_fetch_add((atomic<longlong> *) ui,-1)==0){
	  free(ui);
          ui=0;
        }
#else
        --(*ui);
	if (*ui==0){
	  free(ui);
          ui=0;
        }
#endif
      }
    }
    tdeg_t64 & operator = (const tdeg_t64 & a){
      if (tab[0]%2 && ui){
#ifdef ATOMIC
	if (atomic_fetch_add((atomic<longlong> *) ui,-1)==0){
	  free(ui);
          ui=0;
        }
#else
        --(*ui);
	if (*ui==0){
	  free(ui);
          ui=0;
        }
#endif
	if (a.tab[0] % 2){
	  tdeg=a.tdeg;
	  tdeg2=a.tdeg2;
	  order_=a.order_;
	  ui=a.ui;
#ifdef GIAC_HASH
	  hash=a.hash;
#endif
#ifdef GIAC_ELIM
	  elim=a.elim;
#endif
#ifdef ATOMIC
          atomic_fetch_add((atomic<longlong> *) ui,1);
#else
          ++(*ui);
#endif
	  return *this;
	}
      }
      else {
	if (a.tab[0]%2){
#ifdef ATOMIC
          atomic_fetch_add((atomic<longlong> *) a.ui,1);
#else
          ++(*a.ui);
#endif
	}
      }
      longlong * ptr = (longlong *) tab;
      longlong * aptr = (longlong *) a.tab;
      ptr[0]=aptr[0];
      ptr[1]=aptr[1];
      ptr[2]=aptr[2];
      ptr[3]=aptr[3];
      return *this;
    }
#else
    short tab[GROEBNER_VARS+1];
    int front(){ return tab[1];}
#endif
    // methods
    inline unsigned selection_degree(order_t order) const {
#ifdef GBASIS_SELECT_TOTAL_DEGREE
      return total_degree(order);
#endif
#ifdef GIAC_64VARS
      if (tab[0]%2)
	return tdeg/2;
#endif
      return tdeg;
    }
    inline unsigned total_degree(order_t order) const {
#ifdef GIAC_64VARS
      if (tab[0]%2)
	return tdeg/2+tdeg2;
#endif
      // works only for revlex and tdeg
#if 0
      if (order==_REVLEX_ORDER || order==_TDEG_ORDER)
	return tab[0];
      if (order==_3VAR_ORDER)
	return (tab[0] << 16)+tab[4];
      if (order==_7VAR_ORDER)
	return (tab[0] << 16) +tab[8];
      if (order==_11VAR_ORDER)
	return (tab[0] << 16) +tab[12];
#endif
      return tab[0];
    }
    // void set_total_degree(unsigned d) { tab[0]=d;}
    tdeg_t64() { 
      longlong * ptr = (longlong *) tab;
      ptr[2]=ptr[1]=ptr[0]=0;
#if GROEBNER_VARS>11
      ptr[3]=0;
#endif
    }
    tdeg_t64(int i){
      longlong * ptr = (longlong *) tab;
      ptr[2]=ptr[1]=ptr[0]=0;
#if GROEBNER_VARS>11
      ptr[3]=0;
#endif
    }
    void get_tab(short * ptr,order_t order) const {
#ifdef GIAC_64VARS
      if (tab[0]%2){ // copy only 16 first
	degtype * ptr_=(degtype *)(ui+1);
	for (unsigned i=0;i<=GROEBNER_VARS;++i)
	  ptr[i]=ptr_[i];
	return;
      }
#endif
      for (unsigned i=0;i<=GROEBNER_VARS;++i)
	ptr[i]=tab[i];
#ifdef GIAC_64VARS
      ptr[0]/=2;
#endif
#ifdef GBASIS_SWAP
      swap_indices(ptr);
#endif
    }
    tdeg_t64(const index_m & lm,order_t order){ 
#ifdef GIAC_64VARS
      if (lm.size()>GROEBNER_VARS){
	ui=(longlong *)malloc((1+(lm.size()+degratiom1)/degratio)*sizeof(longlong));
	longlong* ptr=ui;
	*ptr=1; ++ ptr;
#ifdef GIAC_CHARDEGTYPE
	for (int i=0;i<lm.size();){
	  unsigned char tableau[8]={0,0,0,0,0,0,0,0};
	  tableau[0]=lm[i];
	  ++i;
	  if (i<lm.size())
	    tableau[1] = lm[i];
	  ++i;
	  if (i<lm.size())
	    tableau[2]= lm[i];
	  ++i; 
	  if (i<lm.size())
	    tableau[3]= lm[i];
	  ++i; 
	  if (i<lm.size())
	    tableau[4]= lm[i];
	  ++i; 
	  if (i<lm.size())
	    tableau[5]= lm[i];
	  ++i; 
	  if (i<lm.size())
	    tableau[6]= lm[i];
	  ++i; 
	  if (i<lm.size())
	    tableau[7]= lm[i];
	  ++i; 
	  *ptr = * (longlong *)tableau ;
	  ++ptr;
	}
#else
	for (int i=0;i<lm.size();){
	  longlong x=lm[i];
	  ++i;
	  x <<= 16;
	  if (i<lm.size())
	    x += lm[i];
	  ++i;
	  x <<= 16;
	  if (i<lm.size())
	    x += lm[i];
	  ++i;
	  x <<= 16;
	  if (i<lm.size())
	    x += lm[i];
#ifndef BIGENDIAN
	  x = (x>>48) | (((x>>32)&0xffff)<<16) | (((x>>16)&0xffff)<<32) | ((x&0xffff)<<48);
#endif
	  *ptr = x;
	  ++ptr;
	  ++i;
	}
#endif // GIAC_CHARDEGTYPE
	if (order.o==_3VAR_ORDER || order.o>=_7VAR_ORDER){
	  tdeg=2*nvar_total_degree(lm,order.o)+1;
	  tdeg2=sum_degree_from(lm,order.o); 
	}
	else {
	  tdeg=short(2*lm.total_degree()+1);
	  tdeg2=0;
	}
	order_=order;
#if 1 // def GIAC_HASH 
	compute_degs(); // for hash
#else
#ifdef GIAC_ELIM
	ptr=ui+1;
	int firstblock=order_.o;
	if (firstblock!=_3VAR_ORDER && firstblock<_7VAR_ORDER)
	  firstblock=order_.dim;
	compute_elim(ptr,ptr+(firstblock+degratiom1)/degratio);
#endif
#endif
	return;
      }
#endif // GIAC_64VARS
      longlong * ptr_ = (longlong *) tab;
      ptr_[2]=ptr_[1]=ptr_[0]=0;
      short * ptr=tab;
#if GROEBNER_VARS>11
      ptr_[3]=0;
#endif
      // tab[GROEBNER_VARS]=order;
#if GROEBNER_VARS==15
      if (order.o==_3VAR_ORDER){
#ifdef GIAC_64VARS
	ptr[0]=2*(lm[0]+lm[1]+lm[2]);
#else
	ptr[0]=lm[0]+lm[1]+lm[2];
#endif
	ptr[1]=lm[2];
	ptr[2]=lm[1];
	ptr[3]=lm[0];
	ptr +=5;
	short t=0;
	vector<deg_t>::const_iterator it=lm.begin()+3,itend=lm.end();
	for (--itend,--it;it!=itend;++ptr,--itend){
	  t += *itend;
	  *ptr=*itend;
	}
	tab[4]=t;
	return;
      }
      if (order.o==_7VAR_ORDER){
#ifdef GIAC_64VARS
	ptr[0]=2*(lm[0]+lm[1]+lm[2]+lm[3]+lm[4]+lm[5]+lm[6]);
#else
	ptr[0]=lm[0]+lm[1]+lm[2]+lm[3]+lm[4]+lm[5]+lm[6];
#endif
	ptr[1]=lm[6];
	ptr[2]=lm[5];
	ptr[3]=lm[4];
	ptr[4]=lm[3];
	ptr[5]=lm[2];
	ptr[6]=lm[1];
	ptr[7]=lm[0];
	ptr +=9;
	short t=0;
	vector<deg_t>::const_iterator it=lm.begin()+7,itend=lm.end();
	for (--itend,--it;it!=itend;++ptr,--itend){
	  t += *itend;
	  *ptr=*itend;
	}
	tab[8]=t;
	return;
      }
      if (order.o==_11VAR_ORDER){
#ifdef GIAC_64VARS
	ptr[0]=2*(lm[0]+lm[1]+lm[2]+lm[3]+lm[4]+lm[5]+lm[6]+lm[7]+lm[8]+lm[9]+lm[10]);
#else
	ptr[0]=lm[0]+lm[1]+lm[2]+lm[3]+lm[4]+lm[5]+lm[6]+lm[7]+lm[8]+lm[9]+lm[10];
#endif
	ptr[1]=lm[10];
	ptr[2]=lm[9];
	ptr[3]=lm[8];
	ptr[4]=lm[7];
	ptr[5]=lm[6];
	ptr[6]=lm[5];
	ptr[7]=lm[4];
	ptr[8]=lm[3];
	ptr[9]=lm[2];
	ptr[10]=lm[1];
	ptr[11]=lm[0];
	ptr += 13;
	short t=0;
	vector<deg_t>::const_iterator it=lm.begin()+11,itend=lm.end();
	for (--itend,--it;it!=itend;++ptr,--itend){
	  t += *itend;
	  *ptr=*itend;
	}
	tab[12]=t;
	return;
      }
#endif
      vector<deg_t>::const_iterator it=lm.begin(),itend=lm.end();
      if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
	*ptr=sum_degree(lm);
	++ptr;
      }
      if (order.o==_REVLEX_ORDER){
	for (--itend,--it;it!=itend;++ptr,--itend)
	  *ptr=*itend;
      }
      else {
	for (;it!=itend;++ptr,++it)
	  *ptr=*it;
      }
#ifdef GBASIS_SWAP
      swap_indices(tab);
#endif
#ifdef GIAC_64VARS
      *tab *=2;
#endif
    }
  };

  typedef map<tdeg_t64,unsigned> annuaire;

#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const tdeg_t64 & x){
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
      os << "[";
      const longlong * ptr=x.ui+1,*ptrend=ptr+(x.order_.dim+degratiom1)/degratio;
      for (;ptr!=ptrend;++ptr){
	longlong x=*ptr;
#ifdef BIGENDIAN
	os << ((x>>48) &0xffff)<< "," << ((x>>32) & 0xffff) << "," << ((x>>16) & 0xffff) << "," << ((x) & 0xffff) << ",";
#else
	os << ((x) &0xffff)<< "," << ((x>>16) & 0xffff) << "," << ((x>>32) & 0xffff) << "," << ((x>>48) & 0xffff) << ",";
#endif
      }
      return os << "]";
    }
#endif    
    os << "[";
    for (unsigned i=0; i<=GROEBNER_VARS;++i){
      os << x.tab[i] << ",";
    }
    return os << "]";
  }
#else
  ostream & operator << (ostream & os,const tdeg_t64 & x){
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
      // debugging
      tdeg_t64 xsave(x); xsave.compute_degs();
      if (xsave.tdeg!=x.tdeg || xsave.tdeg2!=x.tdeg2)
	os << "degree error " ;
      os << "[";
      const longlong * ptr=x.ui+1,*ptrend=ptr+(x.order_.dim+degratiom1)/degratio;
      for (;ptr!=ptrend;++ptr){
	longlong x=*ptr;
#ifdef BIGENDIAN
	os << ((x>>48) &0xffff)<< "," << ((x>>32) & 0xffff) << "," << ((x>>16) & 0xffff) << "," << ((x) & 0xffff) << ",";
#else
	os << ((x) &0xffff)<< "," << ((x>>16) & 0xffff) << "," << ((x>>32) & 0xffff) << "," << ((x>>48) & 0xffff) << ",";
#endif
      }
      return os << "]";
    }
#endif    
    os << "[";
    for (unsigned i=0; i<=GROEBNER_VARS;++i){
      os << x.tab[i] << ",";
    }
    return os << "]";
  }
#endif
  void tdeg_t64::dbgprint() const { COUT << * this << '\n'; }
  tdeg_t64 operator + (const tdeg_t64 & x,const tdeg_t64 & y);
  tdeg_t64 & operator += (tdeg_t64 & x,const tdeg_t64 & y){ 
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(y.tab[0]%2)){
	y.dbgprint();
	COUT << "erreur" << '\n';
      }
#endif
      return x=x+y;
    }
#ifdef GIAC_DEBUG_TDEG_T64
    if ((y.tab[0]%2)){
      y.dbgprint();
      COUT << "erreur" << '\n';
    }
#endif
#endif    
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    xtab[0]+=ytab[0];
    xtab[1]+=ytab[1];
    xtab[2]+=ytab[2];
#if GROEBNER_VARS>11
    xtab[3]+=ytab[3];
#endif
#else
    for (unsigned i=0;i<=GROEBNER_VARS;++i)
      x.tab[i]+=y.tab[i];
#endif
    return x;  
  }

  inline tdeg_t64 dynamic_plus(const tdeg_t64 & x,const tdeg_t64 & y){
    tdeg_t64 res;
    res.order_=x.order_;
    res.ui=(longlong *)malloc((1+(x.order_.dim+degratiom1)/degratio)*sizeof(longlong));
    res.ui[0]=1; 
    const longlong * xptr=x.ui+1,*xend=xptr+(x.order_.dim+degratiom1)/degratio,*yptr=y.ui+1;
    longlong * resptr=res.ui+1;
    for (;xptr!=xend;++resptr,++yptr,++xptr)
      *resptr=*xptr+*yptr;
#if 1
    res.tdeg=1+2*(x.tdeg/2+y.tdeg/2);
    res.tdeg2=x.tdeg2+y.tdeg2;
#ifdef GIAC_HASH
    res.hash=x.hash+y.hash;
#endif
#ifdef GIAC_ELIM      
    if (res.tdeg>=33)
      res.elim=0x1fffffffffffffffULL;
    else
      res.elim=x.elim+y.elim;
#endif      
#else
    res.tdeg=1;
    res.compute_degs();
#endif
    return res;
  }
  
  tdeg_t64 operator + (const tdeg_t64 & x,const tdeg_t64 & y){ 
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(y.tab[0]%2))
	COUT << "erreur" << '\n';
#endif
      return dynamic_plus(x,y);
    }
#endif    
#ifdef GIAC_DEBUG_TDEG_T64
    if (y.tab[0]%2){
      y.dbgprint();
      COUT << "erreur" << '\n';
    }
#endif
    tdeg_t64 res(x);
    return res += y;
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]+ytab[0];
    ztab[1]=xtab[1]+ytab[1];
    ztab[2]=xtab[2]+ytab[2];
#if GROEBNER_VARS>11
    ztab[3]=xtab[3]+ytab[3];
#endif
#else
    for (unsigned i=0;i<=GROEBNER_VARS;++i)
      res.tab[i]=x.tab[i]+y.tab[i];
#endif
    return res;
  }
  inline void add(const tdeg_t64 & x,const tdeg_t64 & y,tdeg_t64 & res,int dim){
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(y.tab[0]%2))
	COUT << "erreur" << '\n';
#endif
      if (res.tab[0]%2 && res.ui[0]==1){
	const longlong * xptr=x.ui+1,*xend=xptr+(x.order_.dim+degratiom1)/degratio,*yptr=y.ui+1;
	longlong * resptr=res.ui+1;
#ifndef GIAC_CHARDEGTYPE
	*resptr=*xptr+*yptr;++resptr,++yptr,++xptr;
	*resptr=*xptr+*yptr;++resptr,++yptr,++xptr;
	*resptr=*xptr+*yptr;++resptr,++yptr,++xptr;
	*resptr=*xptr+*yptr;++resptr,++yptr,++xptr;
#endif
	for (;xptr!=xend;++resptr,++yptr,++xptr)
	  *resptr=*xptr+*yptr;
#if 1
	res.tdeg=1+2*(x.tdeg/2+y.tdeg/2);
	res.tdeg2=x.tdeg2+y.tdeg2;
#ifdef GIAC_HASH
	res.hash=x.hash+y.hash;
#endif
#ifdef GIAC_ELIM      
	if (res.tdeg>=33)
	  res.elim=0x1fffffffffffffffULL;
	else
	  res.elim=x.elim+y.elim;
#endif      
#else
	res.tdeg=1;
	res.compute_degs();
#endif
      }
      else
	res=dynamic_plus(x,y);
      return;
    }
#endif    
#if 0 // def GIAC_64VARS
    if (x.tab[0]%2){
      res = x;
      res += y;
      return;
    }
#endif    
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]+ytab[0];
    ztab[1]=xtab[1]+ytab[1];
    ztab[2]=xtab[2]+ytab[2];
#if GROEBNER_VARS>11
    ztab[3]=xtab[3]+ytab[3];
#endif
#else
    for (unsigned i=0;i<=dim;++i)
      res.tab[i]=x.tab[i]+y.tab[i];
#endif
  }

  tdeg_t64 operator - (const tdeg_t64 & x,const tdeg_t64 & y){ 
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(y.tab[0]%2))
	COUT << "erreur" << '\n';
#endif
      tdeg_t64 res;
      res.order_=x.order_;
      res.ui=(longlong *)malloc((1+(x.order_.dim+degratiom1)/degratio)*sizeof(longlong));
      res.ui[0]=1; 
      const longlong * xptr=x.ui+1,*xend=xptr+(x.order_.dim+degratiom1)/degratio,*yptr=y.ui+1;
      longlong * resptr=res.ui+1;
      for (;xptr!=xend;++resptr,++yptr,++xptr)
	*resptr=*xptr-*yptr;
      res.tdeg=1;
      res.compute_degs();
      return res;
    }
#ifdef GIAC_DEBUG_TDEG_T64
    if ((y.tab[0]%2)){
      y.dbgprint();
      COUT << "erreur" << '\n';
    }
#endif    
#endif // GIAC_64VARS   
    tdeg_t64 res;
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]-ytab[0];
    ztab[1]=xtab[1]-ytab[1];
    ztab[2]=xtab[2]-ytab[2];
#if GROEBNER_VARS>11
    ztab[3]=xtab[3]-ytab[3];
#endif
#else
    for (unsigned i=0;i<=GROEBNER_VARS;++i)
      res.tab[i]=x.tab[i]-y.tab[i];
#endif
    return res;
  }
  inline bool operator == (const tdeg_t64 & x,const tdeg_t64 & y){ 
    longlong X=((longlong *) x.tab)[0];
    if (X!= ((longlong *) y.tab)[0])
      return false;
#ifdef GIAC_HASH
    if (x.hash!=y.hash) 
      return false;
#endif
#ifdef GIAC_ELIM
    if (x.elim!=y.elim) return false;
#endif
#ifdef GIAC_64VARS
    if (
#ifdef BIGENDIAN
	x.tab[0]%2
#else
	X%2
#endif
	){
      //if (x.ui==y.ui) return true;
      const longlong * xptr=x.ui+1,*xend=xptr+(x.order_.dim+degratiom1)/degratio,*yptr=y.ui+1;
#ifndef GIAC_CHARTABDEG
      // dimension+3 is at least 16 otherwise the alternative code would be called
      if (*xptr!=*yptr)
	return false;
      ++yptr,++xptr;
      if (*xptr!=*yptr)
	return false;
      ++yptr,++xptr;
      if (*xptr!=*yptr)
	return false;
      ++yptr,++xptr;
      if (*xptr!=*yptr)
	return false;
      ++yptr,++xptr;
#endif
      for (;xptr!=xend;++yptr,++xptr){
	if (*xptr!=*yptr)
	  return false;
      }
      return true;
    }
#endif    
    return ((longlong *) x.tab)[1] == ((longlong *) y.tab)[1] &&
      ((longlong *) x.tab)[2] == ((longlong *) y.tab)[2] 
#if GROEBNER_VARS>11
      &&  ((longlong *) x.tab)[3] == ((longlong *) y.tab)[3]
#endif
    ;
  }
  inline bool operator != (const tdeg_t64 & x,const tdeg_t64 & y){ 
    return !(x==y);
  }

  static inline int tdeg_t64_revlex_greater (const tdeg_t64 & x,const tdeg_t64 & y){
#ifdef GBASIS_SWAP
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    if (xtab[0]!=ytab[0]) // tdeg test already donne by caller
      return xtab[0]<=ytab[0]?1:0;
    if (xtab[1]!=ytab[1])
      return xtab[1]<=ytab[1]?1:0;
    if (xtab[2]!=ytab[2])
      return xtab[2]<=ytab[2]?1:0;
#if GROEBNER_VARS>11
    return xtab[3]<=ytab[3]?1:0;
#endif
    return 2;
#else // GBASIS_SWAP
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>=y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]<=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
    }
#if GROEBNER_VARS>11
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]<=y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
    }
#endif
    return 2;
#endif // GBASIS_SWAP
  }

  // inline bool operator <  (const tdeg_t64 & x,const tdeg_t64 & y){ return !tdeg_t64_revlex_greater(x,y); }
  // inline bool operator >  (const tdeg_t64 & x,const tdeg_t64 & y){ return !tdeg_t64_revlex_greater(y,x); }
  // inline bool operator <= (const tdeg_t64 & x,const tdeg_t64 & y){ return tdeg_t64_revlex_greater(y,x); }
  // inline bool operator >=  (const tdeg_t64 & x,const tdeg_t64 & y){ return tdeg_t64_revlex_greater(x,y); }

#if GROEBNER_VARS==15

  int tdeg_t64_3var_greater (const tdeg_t64 & x,const tdeg_t64 & y){
    if (x.tab[0]!=y.tab[0])
      return x.tab[0]>=y.tab[0]?1:0;
    if (x.tab[4]!=y.tab[4])
      return x.tab[4]>=y.tab[4]?1:0;
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]<=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
    }
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]<=y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
    }
    return 2;
  }

  int tdeg_t64_7var_greater (const tdeg_t64 & x,const tdeg_t64 & y){
    if (x.tab[0]!=y.tab[0])
      return x.tab[0]>=y.tab[0]?1:0;
    if (x.tab[8]!=y.tab[8])
      return x.tab[8]>=y.tab[8]?1:0;
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
    }
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]<=y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
    }
    return 2;
  }

  int tdeg_t64_11var_greater (const tdeg_t64 & x,const tdeg_t64 & y){
    if (x.tab[0]!=y.tab[0])
      return x.tab[0]>=y.tab[0]?1:0;
    if (x.tab[12]!=y.tab[12])
      return x.tab[12]>=y.tab[12]?1:0;
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]<=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
    }
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
    }
    return 2;
  }
#endif // GROEBNER_VARS==15

  int tdeg_t64_lex_greater (const tdeg_t64 & x,const tdeg_t64 & y){
#ifdef GBASIS_SWAP
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    ulonglong X=*xtab, Y=*ytab;
    if (X!=Y){
      if ( (X & 0xffff) != (Y &0xffff))
	return (X&0xffff)>=(Y&0xffff)?1:0;
      return X>=Y?1:0;
    }
    if (xtab[1]!=ytab[1])
      return xtab[1]>=ytab[1]?1:0;
    if (xtab[2]!=ytab[2])
      return xtab[2]>=ytab[2]?1:0;
#if GROEBNER_VARS>11
    return xtab[3]>=ytab[3]?1:0;
#endif
    return 2;
#else
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]>y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]>y.tab[2]?1:0;
      return x.tab[3]>y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]>y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]>y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]>y.tab[6]?1:0;
      return x.tab[7]>y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]>y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]>y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]>y.tab[10]?1:0;
      return x.tab[11]>=y.tab[11]?1:0;
    }
#if GROEBNER_VARS>11
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]>y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]>y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]>y.tab[14]?1:0;
      return x.tab[15]>=y.tab[15]?1:0;
    }
#endif
    return 2;
#endif
  }

  inline int tdeg_t_greater_dyn(const tdeg_t64 & x,const tdeg_t64 & y,order_t order){
    const longlong * it1=x.ui,* it2=y.ui,*it1beg;
    longlong a=0;
    switch (order.o){
    case _REVLEX_ORDER:
      it1beg=x.ui;
      it1=x.ui+(x.order_.dim+degratiom1)/degratio;
      it2=y.ui+(y.order_.dim+degratiom1)/degratio;
      it1beg += 3;
      for (;it1>it1beg;){
	a=*it1-*it2;
	if (a)
	  return a<=0?1:0;
        --it2;--it1;
	a=*it1-*it2;
	if (a)
	  return a<=0?1:0;
        --it2;--it1;
	a=*it1-*it2;
	if (a)
	  return a<=0?1:0;
        --it2;--it1;
	a=*it1-*it2;
	if (a)
	  return a<=0?1:0;
        --it2;--it1;
      }
      it1beg -= 3;
      for (;it1!=it1beg;--it2,--it1){
	a=*it1-*it2;
	if (a)
	  return a<=0?1:0;
      }
      return 2;
    case _64VAR_ORDER:
      a=it1[16]-it2[16];
      if (a)
	return a<=0?1:0;
      a=it1[15]-it2[15];
      if (a)
	return a<=0?1:0;
      a=it1[14]-it2[14];
      if (a)
	return a<=0?1:0;
      a=it1[13]-it2[13];
      if (a)
	return a<=0?1:0;
    case _48VAR_ORDER:
      a=it1[12]-it2[12];
      if (a)
	return a<=0?1:0;
      a=it1[11]-it2[11];
      if (a)
	return a<=0?1:0;
      a=it1[10]-it2[10];
      if (a)
	return a<=0?1:0;
      a=it1[9]-it2[9];
      if (a)
	return a<=0?1:0;
    case _32VAR_ORDER:
      a=it1[8]-it2[8];
      if (a)
	return a<=0?1:0;
      a=it1[7]-it2[7];
      if (a)
	return a<=0?1:0;
      a=it1[6]-it2[6];
      if (a)
	return a<=0?1:0;
      a=it1[5]-it2[5];
      if (a)
	return a<=0?1:0;
    case _16VAR_ORDER:
      a=it1[4]-it2[4];
      if (a)
	return a<=0?1:0;
    case _11VAR_ORDER:
      a=it1[3]-it2[3];
      if (a)
	return a<=0?1:0;
    case _7VAR_ORDER:
      a=it1[2]-it2[2];
      if (a)
	return a<=0?1:0;
    case _3VAR_ORDER: 
      a=it1[1]-it2[1];
      if (a)
	return a<=0?1:0;
      if (x.tdeg2!=y.tdeg2)
	return x.tdeg2>y.tdeg2?1:0;
      it1beg=it1+(x.order_.o+degratiom1)/degratio;
      it1 += (x.order_.dim+degratiom1)/degratio;;
      it2 += (x.order_.dim+degratiom1)/degratio;;
      for (;;){
	a=*it1-*it2;
	if (a)
	  return a<=0?1:0;
	--it2;--it1;
	if (it1<=it1beg)
	  return 2;
      }
    case _TDEG_ORDER: case _PLEX_ORDER: {
      const degtype * it1=(degtype *)(x.ui+1),*it1end=it1+x.order_.dim,*it2=(degtype *)(y.ui+1);
      for (;it1!=it1end;++it2,++it1){
	if (*it1!=*it2)
	  return *it1>=*it2?1:0;
      }
      return 2;
    }
    } // end swicth
    return -1;
  }

  inline int tdeg_t_greater(const tdeg_t64 & x,const tdeg_t64 & y,order_t order){
    short X=x.tab[0];
    if (order.o!=_PLEX_ORDER && X!=y.tab[0]) return X>y.tab[0]?1:0; // since tdeg is tab[0] for plex
#ifdef GIAC_64VARS
    if (X%2){
      if (order.o!=_PLEX_ORDER && x.tdeg2!=y.tdeg2) return x.tdeg2>y.tdeg2?1:0;
#ifdef GIAC_ELIM
      if ( x.elim!=y.elim) return x.elim<y.elim?1:0;
#endif
      //if (x.ui==y.ui) return 2;
#if 1 && !defined BIGENDIAN && !defined GIAC_CHARDEGTYPE
      return tdeg_t_greater_dyn(x,y,order);
#else // BIGENDIAN, GIAC_CHARDEGTYPE
      if (order.o>=_7VAR_ORDER || order.o==_3VAR_ORDER){
	int n=(order.o+degratiom1)/degratio;
	const longlong * it1beg=x.ui,*it1=x.ui+n,*it2=y.ui+n;
	longlong a=0,b=0;
#ifdef BIGENDIAN
	for (;it1!=it1beg;--it2,--it1){
	  a=*it1; 
	  b=*it2;
	  if (a!=b)
	    break;
	}
	if (a!=b){
	  if ( ((a)&0xffff) != ((b)&0xffff) )
	    return ((a)&0xffff) <= ((b)&0xffff)?1:0;
	  if ( ((a>>16)&0xffff) != ((b>>16)&0xffff) )
	    return ((a>>16)&0xffff) <= ((b>>16)&0xffff)?1:0;
	  if ( ((a>>32)&0xffff) != ((b>>32)&0xffff) )
	    return ((a>>32)&0xffff) <= ((b>>32)&0xffff)?1:0;
	  return a <= b?1:0;
	}
#else
	for (;it1!=it1beg;--it2,--it1){
	  a=*it1-*it2;
	  if (a)
	    return a<=0?1:0;
	}
#endif
	// if (x.tdeg2!=y.tdeg2) return x.tdeg2>=y.tdeg2;
	it1beg=x.ui+n;
	n=(x.order_.dim+degratiom1)/degratio;
	it1=x.ui+n;
	it2=y.ui+n;
#ifdef BIGENDIAN
	for (a=0,b=0;it1!=it1beg;--it2,--it1){
	  a=*it1; b=*it2;
	  if (a!=b)
	    break;
	}
	if (a!=b){
	  if ( ((a)&0xffff) != ((b)&0xffff) )
	    return ((a)&0xffff) <= ((b)&0xffff)?1:0;
	  if ( ((a>>16)&0xffff) != ((b>>16)&0xffff) )
	    return ((a>>16)&0xffff) <= ((b>>16)&0xffff) ?1:0;
	  if ( ((a>>32)&0xffff) != ((b>>32)&0xffff) )
	    return ((a>>32)&0xffff) <= ((b>>32)&0xffff) ?1:0;
	  return a <= b?1:0;
	}
#else
	for (;it1!=it1beg;--it2,--it1){
	  a=*it1-*it2;
	  if (a)
	    return a<=0?1:0;
	}
#endif
	return 2;
      }
      if (order.o==_REVLEX_ORDER){
	//if (x.tdeg!=y.tdeg) return x.tdeg>y.tdeg?1:0;
	const longlong * it1beg=x.ui,*it1=x.ui+(x.order_.dim+degratiom1)/degratio,*it2=y.ui+(y.order_.dim+degratiom1)/degratio;
	longlong a=0,b=0;
#ifdef BIGENDIAN
	for (;it1!=it1beg;--it2,--it1){
	  a=*it1; b=*it2;
	  if (a!=b)
	    break;
	}
	if (a!=b){
	  if ( ((a)&0xffff) != ((b)&0xffff) )
	    return ((a)&0xffff) <= ((b)&0xffff)?1:0;
	  if ( ((a>>16)&0xffff) != ((b>>16)&0xffff) )
	    return ((a>>16)&0xffff) <= ((b>>16)&0xffff)?1:0;
	  if ( ((a>>32)&0xffff) != ((b>>32)&0xffff) )
	    return ((a>>32)&0xffff) <= ((b>>32)&0xffff)?1:0;
	  return a <= b?1:0;
	}
#else
	for (;it1!=it1beg;--it2,--it1){
	  a=*it1-*it2;
	  if (a)
	    return a<=0?1:0;
	}
#endif
	return 2;
      }
      // plex and tdeg share the same code since total degree already checked
      const degtype * it1=(degtype *)(x.ui+1),*it1end=it1+x.order_.dim,*it2=(degtype *)(y.ui+1);
      for (;it1!=it1end;++it2,++it1){
	if (*it1!=*it2)
	  return *it1>=*it2?1:0;
      }
      return 2;
#endif // BIGENDIAN, GIAC_CHARDEGTYPE
    } // end if X%2
#endif // GIAC_64VARS
    if (order.o==_REVLEX_ORDER)
      return tdeg_t64_revlex_greater(x,y);
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER)
      return tdeg_t64_3var_greater(x,y);
    if (order.o==_7VAR_ORDER)
      return tdeg_t64_7var_greater(x,y);
    if (order.o==_11VAR_ORDER)
      return tdeg_t64_11var_greater(x,y);
#endif
    return tdeg_t64_lex_greater(x,y);
  }
  inline bool tdeg_t_strictly_greater (const tdeg_t64 & x,const tdeg_t64 & y,order_t order){
    return !tdeg_t_greater(y,x,order); // total order
  }

  inline bool tdeg_t_all_greater(const tdeg_t64 & x,const tdeg_t64 & y,order_t order){
    if ((*((ulonglong*)&x)-*((ulonglong*)&y)) & 0x8000800080008000ULL)
      return false;
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
#ifdef GIAC_ELIM
      //bool debug=false;
      if ( !( (x.elim | y.elim) & 0x1000000000000000ULL) &&
#if 0 // ndef BESTA_OS // Keil compiler, for some reason does not like the 0b syntax!
	   ( (x.elim-y.elim) & 0b1111100001000010000100001000010000100001000010000100001000010000ULL) )
#else
	( (x.elim-y.elim) & 0xf842108421084210ULL) )
#endif
      {
	//debug=true;
	return false;
      }
#endif
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(y.tab[0]%2))
	COUT << "erreur" << '\n';
#endif
#ifdef GIAC_HASH
      if (x.hash<y.hash)
	return false;
#endif
      const longlong * it1=x.ui+1,*it1end=it1+(x.order_.dim+degratiom1)/degratio,*it2=y.ui+1;
#ifdef GIAC_CHARDEGTYPE
      for (;it1!=it1end;++it2,++it1){
	if ((*it1-*it2) & 0x8080808080808080ULL)
	  return false;
      }
#else
      it1end -=4;
      for (;it1<=it1end;){
        if ((*it1-*it2) & 0x8000800080008000ULL)
          return false;
        ++it2; ++it1;
        if ((*it1-*it2) & 0x8000800080008000ULL)
          return false;
        ++it2; ++it1;
        if ((*it1-*it2) & 0x8000800080008000ULL)
          return false;
        ++it2; ++it1;
        if ((*it1-*it2) & 0x8000800080008000ULL)
          return false;
        ++it2; ++it1;
      }
      it1end+=4;
      for (;it1!=it1end;++it2,++it1){
	if ((*it1-*it2) & 0x8000800080008000ULL)
	  return false;
      }
#endif
      // if (debug) CERR << x << " " << y << '\n' << x.elim << " " << y.elim << " " << x.tdeg << " " << y.tdeg << '\n';
      return true;
    }
#endif
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
  // if ((xtab[0]-ytab[0]) & 0x8000800080008000ULL) return false;
    if ((xtab[1]-ytab[1]) & 0x8000800080008000ULL)
      return false;
    if ((xtab[2]-ytab[2]) & 0x8000800080008000ULL)
      return false;
#if GROEBNER_VARS>11
    if ((xtab[3]-ytab[3]) & 0x8000800080008000ULL)
      return false;
#endif
    return true;
  }

  const longlong mask2=0x8080808080808080ULL;
#ifdef GIAC_CHARDEGTYPE
    const longlong mask=0x8080808080808080ULL;
#else
    const longlong mask=0x8000800080008000ULL;
#endif

  // 1 (all greater), 0 (unknown), -1 (all smaller)
  int tdeg_t_compare_all(const tdeg_t64 & x,const tdeg_t64 & y,order_t order){
#ifdef GIAC_64VARS
    if (x.tab[0]%2){
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(y.tab[0]%2))
	COUT << "erreur" << '\n';
#endif
      if ( (x.tdeg<y.tdeg) ^ (x.tdeg2<y.tdeg2))
	return 0;
      const longlong * it1=x.ui+1,*it1end=it1+(x.order_.dim+degratiom1)/degratio,*it2=y.ui+1;
      int res=0;
      for (;it1!=it1end;++it2,++it1){
	longlong tmp=*it1-*it2;
	if (tmp & mask){
	  if (res==1 || ((-tmp) & mask)) return 0;
	  res=-1;
	}
	else {
	  if (res==-1) return 0; else res=1;
	}
      }
      return res;
    }
#endif // GIAC_64VARS
    int res=0;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    longlong tmp=xtab[0]-ytab[0];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    tmp=xtab[1]-ytab[1];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    tmp=xtab[2]-ytab[2];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
#if GROEBNER_VARS>11
    tmp=xtab[3]-ytab[3];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
#endif
    return res;
  }

  void index_lcm(const tdeg_t64 & x,const tdeg_t64 & y,tdeg_t64 & z,order_t order){
#ifdef GIAC_64VARS
    if (x.tdeg%2){
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(y.tab[0]%2))
	COUT << "erreur" << '\n';
#endif
      z=tdeg_t64();
      z.tdeg=1;
      z.order_=x.order_;
      int nbytes=(1+(x.order_.dim+degratiom1)/degratio)*sizeof(longlong);
      z.ui=(longlong *)malloc(nbytes);
      z.ui[0]=1;
      const degtype * xptr=(degtype *)(x.ui+1),*xend=xptr+degratio*((x.order_.dim+degratiom1)/degratio),*yptr=(degtype *)(y.ui+1);
      degtype * resptr=(degtype *)(z.ui+1);
      for (;xptr!=xend;++resptr,++yptr,++xptr)
	*resptr=*xptr>*yptr?*xptr:*yptr;
      z.tdeg=1;
      z.compute_degs();
      return ;
    }
#endif
    int t=0;
    const short * xtab=&x.tab[1],*ytab=&y.tab[1];
    short *ztab=&z.tab[1];
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 1
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 2
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 3
    ++xtab; ++ytab; ++ztab;
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER){
#ifdef GIAC_64VARS
      z.tab[0]=2*t;
#else
      z.tab[0]=t;
#endif
      t=0;
      ++xtab;++ytab;++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 5
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 6
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 7
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 8
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 12
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
      z.tab[4]=t; // 4
      return;
    }
#endif
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 4
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 5
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 6
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 7
    ++xtab; ++ytab; ++ztab;
#if GROEBNER_VARS==15
    if (order.o==_7VAR_ORDER){
#ifdef GIAC_64VARS
      z.tab[0]=2*t;
#else
      z.tab[0]=t;
#endif
      t=0;
      ++xtab;++ytab;++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 12
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
      z.tab[8]=t; // 8
      return;
    }
#endif
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 8
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
#if GROEBNER_VARS>11
    ++xtab; ++ytab; ++ztab;
#if GROEBNER_VARS==15
    if (order.o==_11VAR_ORDER){
#ifdef GIAC_64VARS
      z.tab[0]=2*t;
#else
      z.tab[0]=t;
#endif
      t=0;
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
      z.tab[12]=t; // 12
      return;
    }
#endif
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 12
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
#endif
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
#ifdef GIAC_64VARS
      z.tab[0]=2*t;
#else
      z.tab[0]=t;
#endif
    }
    else {
#ifdef GIAC_64VARS
      z.tab[0]=2*((x.tab[0]>y.tab[0])?x.tab[0]:y.tab[0]);
#else
      z.tab[0]=(x.tab[0]>y.tab[0])?x.tab[0]:y.tab[0];
#endif
    }
  }

  void index_lcm_overwrite(const tdeg_t64 & x,const tdeg_t64 & y,tdeg_t64 & z,order_t order){
    if (z.tdeg%2==0){
      index_lcm(x,y,z,order);
      return;
    }
    const degtype * xptr=(degtype *)(x.ui+1),*xend=xptr+degratio*((x.order_.dim+degratiom1)/degratio),*yptr=(degtype *)(y.ui+1);
    degtype * resptr=(degtype *)(z.ui+1);
    for (;xptr!=xend;++resptr,++yptr,++xptr)
      *resptr=*xptr>*yptr?*xptr:*yptr;
    z.tdeg=1;
    z.compute_degs();
  }
  
  void get_index(const tdeg_t64 & x_,index_t & idx,order_t order,int dim){
#ifdef GIAC_64VARS
    if (x_.tab[0]%2){
      idx.resize(dim);
      if (dim && sizeof(degtype)==sizeof(idx.front())){
        memcpy(&idx.front(),x_.ui+1,dim*sizeof(degtype));
        return;
      }
      const degtype * ptr=(degtype *)(x_.ui+1),*ptrend=ptr+x_.order_.dim;
      index_t::iterator target=idx.begin();
      for (;ptr!=ptrend;++target,++ptr)
	*target=*ptr;
      return;
    }
#endif
    idx.resize(dim);
#ifdef GBASIS_SWAP    
    tdeg_t64 x(x_);
    swap_indices(x.tab);
#else
    const tdeg_t64 & x= x_;
#endif
    const short * ptr=x.tab;
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER){
      ++ptr;
      for (int i=1;i<=3;++ptr,++i)
	idx[3-i]=*ptr;
      ++ptr;
      for (int i=1;i<=dim-3;++ptr,++i)
	idx[dim-i]=*ptr;
      return;
    }
    if (order.o==_7VAR_ORDER){
      ++ptr;
      for (int i=1;i<=7;++ptr,++i)
	idx[7-i]=*ptr;
      ++ptr;
      for (int i=1;i<=dim-7;++ptr,++i)
	idx[dim-i]=*ptr;
      return;
    }
    if (order.o==_11VAR_ORDER){
      ++ptr;
      for (int i=1;i<=11;++ptr,++i)
	idx[11-i]=*ptr;
      ++ptr;
      for (int i=1;i<=dim-11;++ptr,++i)
	idx[dim-i]=*ptr;
      return;
    }
#endif
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER)
      ++ptr;
    if (order.o==_REVLEX_ORDER){
      for (int i=1;i<=dim;++ptr,++i)
	idx[dim-i]=*ptr;
    }
    else {
      for (int i=0;i<dim;++ptr,++i)
	idx[i]=*ptr;
#ifdef GIAC_64VARS
      idx[0]/=2;
#endif
    }
  }
  
  bool disjoint(const tdeg_t64 & a,const tdeg_t64 & b,order_t order,short dim){
#ifdef GIAC_64VARS
    if (a.tab[0]%2){
#ifdef GIAC_DEBUG_TDEG_T64
      if (!(b.tab[0]%2))
	COUT << "erreur" << '\n';
#endif
      const degtype * xptr=(degtype *)(a.ui+1),*xend=xptr+dim,*yptr=(degtype *)(b.ui+1);
      for (;xptr!=xend;++yptr,++xptr){
	if (*xptr && *yptr)
	  return false;
      }
      return true;
    }
#endif
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER){
      if ( (a.tab[1] && b.tab[1]) ||
	   (a.tab[2] && b.tab[2]) ||
	   (a.tab[3] && b.tab[3]) ||
	   (a.tab[5] && b.tab[5]) ||
	   (a.tab[6] && b.tab[6]) ||
	   (a.tab[7] && b.tab[7]) ||
	   (a.tab[8] && b.tab[8]) ||
	   (a.tab[9] && b.tab[9]) ||
	   (a.tab[10] && b.tab[10]) ||
	   (a.tab[11] && b.tab[11]) ||
	   (a.tab[12] && b.tab[12]) ||
	   (a.tab[13] && b.tab[13]) ||
	   (a.tab[14] && b.tab[14]) ||
	   (a.tab[15] && b.tab[15]) )
	return false;
      return true;
    }
    if (order.o==_7VAR_ORDER){
      if ( (a.tab[1] && b.tab[1]) ||
	   (a.tab[2] && b.tab[2]) ||
	   (a.tab[3] && b.tab[3]) ||
	   (a.tab[4] && b.tab[4]) ||
	   (a.tab[5] && b.tab[5]) ||
	   (a.tab[6] && b.tab[6]) ||
	   (a.tab[7] && b.tab[7]) ||
	   (a.tab[9] && b.tab[9]) ||
	   (a.tab[10] && b.tab[10]) ||
	   (a.tab[11] && b.tab[11]) ||
	   (a.tab[12] && b.tab[12]) ||
	   (a.tab[13] && b.tab[13]) ||
	   (a.tab[14] && b.tab[14]) ||
	   (a.tab[15] && b.tab[15]) )
	return false;
      return true;
    }
    if (order.o==_11VAR_ORDER){
      if ( (a.tab[1] && b.tab[1]) ||
	   (a.tab[2] && b.tab[2]) ||
	   (a.tab[3] && b.tab[3]) ||
	   (a.tab[4] && b.tab[4]) ||
	   (a.tab[5] && b.tab[5]) ||
	   (a.tab[6] && b.tab[6]) ||
	   (a.tab[7] && b.tab[7]) ||
	   (a.tab[8] && b.tab[8]) ||
	   (a.tab[9] && b.tab[9]) ||
	   (a.tab[10] && b.tab[10]) ||
	   (a.tab[11] && b.tab[11]) ||
	   (a.tab[13] && b.tab[13]) ||
	   (a.tab[14] && b.tab[14]) ||
	   (a.tab[15] && b.tab[15]))
	return false;
      return true;
    }
#endif
    const short * it=a.tab, * jt=b.tab;
#ifdef GBASIS_SWAP
    const short * itend=it+GROEBNER_VARS+1;
#endif
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
      ++it; ++jt;
    }
#ifndef GBASIS_SWAP
    const short * itend=it+dim;
#endif
    for (;it<itend;++jt,++it){
      if (*it && *jt)
	return false;
    }
    return true;
  }

  // polynomial are vector< T_unsigned<gen,tdeg_t> >
  template<class tdeg_t>
  struct poly8 {
    std::vector< T_unsigned<gen,tdeg_t> > coord;
    // lex order is implemented using tdeg_t as a list of degrees
    // tdeg uses total degree 1st then partial degree in lex order, max 7 vars
    // revlex uses total degree 1st then opposite of partial degree in reverse ordre, max 7 vars
    order_t order; // _PLEX_ORDER, _REVLEX_ORDER or _TDEG_ORDER or _7VAR_ORDER or _11VAR_ORDER
    short int dim;
    unsigned sugar;
    double logz; // unused, it's here for tripolymod_tri
    int age; // unused, it's here for tripolymod_tri
    void dbgprint() const;
    poly8():dim(0),sugar(0),logz(0),age(-1) {order.o=_PLEX_ORDER; order.lex=0; order.dim=0;}
    poly8(order_t o_,int dim_): order(o_),dim(dim_),sugar(0),logz(0),age(-1) {order.dim=dim_;}
    poly8(const polynome & p,order_t o_){
      order=o_;
      dim=p.dim;
      order.dim=p.dim;
      logz=0;
      age=-1;
      if (order.o%4!=3){
	if (p.is_strictly_greater==i_lex_is_strictly_greater)
	  order.o=_PLEX_ORDER;
	if (p.is_strictly_greater==i_total_revlex_is_strictly_greater)
	  order.o=_REVLEX_ORDER;
	if (p.is_strictly_greater==i_total_lex_is_strictly_greater)
	  order.o=_TDEG_ORDER;
      }
      if (
#ifdef GIAC_64VARS
	  0 &&
#endif
	  p.dim>GROEBNER_VARS)
	CERR << "Number of variables is too large to be handled by giac";
      else {
	coord.reserve(p.coord.size());
	for (unsigned i=0;i<p.coord.size();++i){
	  coord.push_back(T_unsigned<gen,tdeg_t>(p.coord[i].value,tdeg_t(p.coord[i].index,order)));
	}
      }
      if (coord.empty())
	sugar=0;
      else
	sugar=coord.front().u.total_degree(order);
    }
    void get_polynome(polynome & p) const {
      p.dim=dim;
      switch (order.o){
      case _REVLEX_ORDER:
	p.is_strictly_greater=i_total_revlex_is_strictly_greater;
	break;
      case _3VAR_ORDER:
	p.is_strictly_greater=i_3var_is_strictly_greater;
	break;
      case _7VAR_ORDER:
	p.is_strictly_greater=i_7var_is_strictly_greater;
	break;
      case _11VAR_ORDER:
	p.is_strictly_greater=i_11var_is_strictly_greater;
	break;
      case _TDEG_ORDER:
	p.is_strictly_greater=i_total_lex_is_strictly_greater;
	break;
      default:
      case _PLEX_ORDER:
	p.is_strictly_greater=i_lex_is_strictly_greater;
	break;
      }
      p.coord.clear();
      p.coord.reserve(coord.size());
      index_t idx(dim);
      for (unsigned i=0;i<coord.size();++i){
	get_index(coord[i].u,idx,order,dim);
	p.coord.push_back(monomial<gen>(coord[i].g,idx));
      }
      // if (order==_3VAR_ORDER || order==_7VAR_ORDER || order==_11VAR_ORDER) p.tsort();
    }
  };
  template<class tdeg_t>
  bool operator == (const poly8<tdeg_t> & p,const poly8<tdeg_t> &q){
    if (p.coord.size()!=q.coord.size())
      return false;
    for (unsigned i=0;i<p.coord.size();++i){
      if (p.coord[i].u!=q.coord[i].u || p.coord[i].g!=q.coord[i].g)
	return false;
    }
    return true;
  }

#ifdef NSPIRE
  template<class tdeg_t,class T> nio::ios_base<T> & operator << (nio::ios_base<T> & os, const poly8<tdeg_t> & p)
#else
  template<class tdeg_t>
    ostream & operator << (ostream & os, const poly8<tdeg_t> & p)
#endif
  {
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    int t2;
    if (it==itend)
      return os << 0 ;
    for (;it!=itend;){
      os << it->g  ;
#ifndef GBASIS_NO_OUTPUT
      if (it->u.vars64()){
	if (it->u.tdeg%2){
	  degtype * i=(degtype *)(it->u.ui+1);
	  int s=it->u.order_.dim;
	  for (int j=0;j<s;++j){
	    t2=i[j];
	    if (t2)
	      os << "*x"<< j << "^" << t2  ;
	  }
	  ++it;
	  if (it==itend)
	    break;
	  os << " + ";
	  continue;
	}
      }
#endif
      short tab[GROEBNER_VARS+1];
      it->u.get_tab(tab,p.order);
      switch (p.order.o){
      case _PLEX_ORDER:
	for (int i=0;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2)
	    os << "*x"<< i << "^" << t2  ;
	}
	break;
      case _REVLEX_ORDER:
	for (int i=1;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;
#if GROEBNER_VARS==15
      case _3VAR_ORDER:
	for (int i=1;i<=3;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 3-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=5;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 7+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
      case _7VAR_ORDER:
	for (int i=1;i<=7;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 7-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=9;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 11+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
      case _11VAR_ORDER:
	for (int i=1;i<=11;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 11-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=13;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 15+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
#endif
      case _TDEG_ORDER:
	for (int i=1;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  if (t2)
	    os << "*x"<< i-1 << "^" << t2  ;
	}
	break;
      }
      ++it;
      if (it==itend)
	break;
      os << " + ";
    }
    return os;
  }


  template<class tdeg_t>
  void poly8<tdeg_t>::dbgprint() const { 
    CERR << *this << '\n';
  }

  template<class tdeg_t>
  class vectpoly8:public vector<poly8<tdeg_t> >{
  public:
    void dbgprint() const { CERR << *this << '\n'; }
  };

  template<class tdeg_t>
  void vectpoly_2_vectpoly8(const vectpoly & v,order_t order,vectpoly8<tdeg_t> & v8){
    v8.clear();     
    v8.reserve(v.size()); if (debug_infolevel>1000){ v8.dbgprint(); v8[0].dbgprint();}
    for (unsigned i=0;i<v.size();++i){
      v8.push_back(poly8<tdeg_t>(v[i],order));
    }
  }

  template<class tdeg_t>
  void vectpoly8_2_vectpoly(const vectpoly8<tdeg_t> & v8,vectpoly & v){
    v.clear();
    v.reserve(v8.size());
    for (unsigned i=0;i<v8.size();++i){
      v.push_back(polynome(v8[i].dim));
      v8[i].get_polynome(v[i]);
    }
  }

  // Groebner basis code begins here

  template<class tdeg_t>
  gen inplace_ppz(poly8<tdeg_t> & p,bool divide=true,bool quick=false){
    typename vector< T_unsigned<gen,tdeg_t> >::iterator it=p.coord.begin(),itend=p.coord.end();
    if (it==itend)
      return 1;
    gen res=(itend-1)->g;
    for (;it!=itend;++it){
      if (it->g.type==_INT_){
	res=it->g;
	if (quick)
	  return 1;
	break;
      }
    }
    if (res.type==_ZINT)
      res=*res._ZINTptr; // realloc for inplace gcd
    for (it=p.coord.begin();it!=itend;++it){
      if (res.type==_ZINT && it->g.type==_ZINT){
	mpz_gcd(*res._ZINTptr,*res._ZINTptr,*it->g._ZINTptr);
      }
      else
	res=gcd(res,it->g);
      if (is_one(res))
	return 1;
    }
    if (!divide)
      return res;
#ifndef USE_GMP_REPLACEMENTS
    if (res.type==_INT_ && res.val>0){
      for (it=p.coord.begin();it!=itend;++it){
	if (it->g.type!=_ZINT || it->g.ref_count()>1)
	  it->g=it->g/res; 
	else
	  mpz_divexact_ui(*it->g._ZINTptr,*it->g._ZINTptr,res.val);
      }
      return res;
    }
    if (res.type==_ZINT){
      for (it=p.coord.begin();it!=itend;++it){
	if (it->g.type!=_ZINT || it->g.ref_count()>1)
	  it->g=it->g/res; 
	else
	  mpz_divexact(*it->g._ZINTptr,*it->g._ZINTptr,*res._ZINTptr);
      }
      return res;
    }
#endif
    for (it=p.coord.begin();it!=itend;++it){
      it->g=it->g/res; 
    }
    return res;
  }

  template<class tdeg_t>
  void inplace_mult(const gen & g,vector< T_unsigned<gen,tdeg_t> > & v){
    typename std::vector< T_unsigned<gen,tdeg_t> >::iterator it1=v.begin(),it1end=v.end();
    for (;it1!=it1end;++it1){
#if 0
      it1->g=g*(it1->g);
#else
      type_operator_times(g,it1->g,it1->g);
#endif
    }
  }
  
#define GBASIS_HEAP
#ifdef GBASIS_HEAP
  // heap: remove bitfields if that's not enough
  template<class tdeg_t>
  struct heap_t {
    unsigned i:16; // index in pairs of quotients/divisors
    unsigned qi:24;
    unsigned gj:24; // monomial index for quotient and divisor
    tdeg_t u; // product
  };

  // inline bool operator > (const heap_t & a,const heap_t & b){ return a.u>b.u; }

  // inline bool operator < (const heap_t & a,const heap_t & b){ return b>a; }
  template<class tdeg_t>
  struct heap_t_compare {
    order_t order;
    const heap_t<tdeg_t> * ptr;
    inline bool operator () (unsigned a,unsigned b){
      return !tdeg_t_greater((ptr+a)->u,(ptr+b)->u,order);
      // return (ptr+a)->u<(ptr+b)->u;
    }
    heap_t_compare(const vector<heap_t<tdeg_t> > & v,order_t o):order(o),ptr(v.empty()?0:&v.front()){};
  };

  template<class tdeg_t>
  struct compare_heap_t {
    order_t order;
    inline bool operator () (const heap_t<tdeg_t> & a,const heap_t<tdeg_t> & b){
      return !tdeg_t_greater(a.u,b.u,order);
      // return (ptr+a)->u<(ptr+b)->u;
    }
    compare_heap_t(order_t o):order(o) {}
  };

  template<class tdeg_t>
  struct heap_t_ptr {
    heap_t<tdeg_t> * ptr;
  };

  template<class tdeg_t>
  void heap_reduce(const poly8<tdeg_t> & f,const vectpoly8<tdeg_t> & g,const vector<unsigned> & G,unsigned excluded,vectpoly8<tdeg_t> & q,poly8<tdeg_t> & rem,poly8<tdeg_t>& R,gen & s,environment * env){
    // divides f by g[G[0]] to g[G[G.size()-1]] except maybe g[G[excluded]]
    // first implementation: use quotxsient heap for all quotient/divisor
    // do not use heap chain
    // ref Monaghan Pearce if g.size()==1
    // R is a temporary polynomial, should be different from f
    if (&rem==&f){
      R.dim=f.dim; R.order=f.order;
      heap_reduce(f,g,G,excluded,q,R,R,s,env);
      swap(rem.coord,R.coord);
      if (debug_infolevel>1000)
	g.dbgprint(); // instantiate dbgprint()
      return;
    }
    rem.coord.clear();
    if (f.coord.empty())
      return ;
    if (q.size()<G.size())
      q.resize(G.size());
    unsigned guess=0;
    for (unsigned i=0;i<G.size();++i){
      q[i].dim=f.dim;
      q[i].order=f.order;
      q[i].coord.clear();
      guess += unsigned(g[G[i]].coord.size());
    }
    vector<heap_t<tdeg_t> > H;
    compare_heap_t<tdeg_t> key(f.order);
    H.reserve(guess);
    vecteur invlcg(G.size());
    if (env && env->moduloon){
      for (unsigned i=0;i<G.size();++i){
	invlcg[i]=invmod(g[G[i]].coord.front().g,env->modulo);
      }
    }
    s=1;
    gen c,numer,denom;
    longlong C;
    unsigned k=0,i; // k=position in f
    tdeg_t m;
    bool finish=false;
    bool small0=env && env->moduloon && env->modulo.type==_INT_ && env->modulo.val;
    int p=env?env->modulo.val:0;
    while (!H.empty() || k<f.coord.size()){
      // is highest remaining degree in f or heap?
      if (k<f.coord.size() && (H.empty() || tdeg_t_greater(f.coord[k].u,H.front().u,f.order)) ){
	// it's in f or both
	m=f.coord[k].u;
	if (small0)
	  C=smod(f.coord[k].g,p).val;
	else {
	  if (s==1)
	    c=f.coord[k].g;
	  else
	    c=s*f.coord[k].g;
	}
	++k;
      }
      else {
	m=H.front().u;
	c=0;
	C=0;
      }
      // extract from heap all terms having m as monomials, subtract from c
      while (!H.empty() && H.front().u==m){
	std::pop_heap(H.begin(),H.end(),key);
	heap_t<tdeg_t> & current=H.back(); // was root node of the heap
	const poly8<tdeg_t> & gcurrent = g[G[current.i]];
	if (small0)
	  C -= extend(q[current.i].coord[current.qi].g.val) * smod(gcurrent.coord[current.gj].g,p).val; 
	else {
	  if (env && env->moduloon){
	    c -= q[current.i].coord[current.qi].g * gcurrent.coord[current.gj].g;
	  }
	  else {
	    fxnd(q[current.i].coord[current.qi].g,numer,denom);
	    if (denom==s)
	      c -= numer*gcurrent.coord[current.gj].g;
	    else {
	      if (denom==1)
		c -= s*numer*gcurrent.coord[current.gj].g;
	      else
		c -= (s/denom)*numer*gcurrent.coord[current.gj].g;
	    }
	  }
	}
	if (current.gj<gcurrent.coord.size()-1){
	  ++current.gj;
	  current.u=q[current.i].coord[current.qi].u+gcurrent.coord[current.gj].u;
	  push_heap(H.begin(),H.end(),key);
	}
	else
	  H.pop_back();
      }
      if (small0){
	C %= p;
	if (C==0)
	  continue;
	c=C;
      }
      else {
	if (env && env->moduloon)
	  c=smod(c,env->modulo);
	if (c==0)
	  continue;
      }
      // divide (c,m) by one of the g if possible, otherwise push in remainder
      if (finish)
	i=unsigned(G.size());
      else {
	finish=true;
	for (i=0;i<G.size();++i){
	  if (i==excluded || g[G[i]].coord.empty())
	    continue;
	  if (tdeg_t_greater(m,g[G[i]].coord.front().u,f.order)){
	    finish=false;
	    if (tdeg_t_all_greater(m,g[G[i]].coord.front().u,f.order))
	      break;
	  }
	}
      }
      if (i==G.size()){
	if (s==1)
	  rem.coord.push_back(T_unsigned<gen,tdeg_t>(c,m)); // add c/s*m to remainder
	else {
	  //rem.coord.push_back(T_unsigned<gen,tdeg_t>(c/s,m)); // add c/s*m to remainder
	  rem.coord.push_back(T_unsigned<gen,tdeg_t>(Tfraction<gen>(c,s),m)); // add c/s*m to remainder
	}
	continue;
      }
      // add c/s*m/leading monomial of g[G[i]] to q[i]
      tdeg_t monom=m-g[G[i]].coord.front().u;
      if (env && env->moduloon){
	if (invlcg[i]!=1){
	  if (invlcg[i]==-1)
	    c=-c;
	  else
	    c=smod(c*invlcg[i],env->modulo);
	}
	q[i].coord.push_back(T_unsigned<gen,tdeg_t>(c,monom));
      }
      else {
	gen lcg=g[G[i]].coord.front().g;
	gen pgcd=simplify3(lcg,c);
	if (is_positive(-lcg,context0)){
	  lcg=-lcg;
	  c=-c;
	}
	s=s*lcg;
	if (s==1)
	  q[i].coord.push_back(T_unsigned<gen,tdeg_t>(c,monom));
	else
	  q[i].coord.push_back(T_unsigned<gen,tdeg_t>(Tfraction<gen>(c,s),monom));
      }
      // push in heap
      if (g[G[i]].coord.size()>1){
	heap_t<tdeg_t> current={i,int(q[i].coord.size())-1,1,g[G[i]].coord[1].u+monom};
	H.push_back(current);
	push_heap(H.begin(),H.end(),key);
      }
    } // end main heap pseudo-division loop
  }

  template<class tdeg_t>
  void heap_reduce(const poly8<tdeg_t> & f,const vectpoly8<tdeg_t> & g,const vector<unsigned> & G,unsigned excluded,vectpoly8<tdeg_t> & q,poly8<tdeg_t> & rem,poly8<tdeg_t>& TMP1,environment * env){
    gen s;
    if (debug_infolevel>2)
      CERR << f << " = " << '\n';
    heap_reduce(f,g,G,excluded,q,rem,TMP1,s,env);
    // end up by multiplying rem by s (so that everything is integer)
    if (debug_infolevel>2){
      for (unsigned i=0;i<G.size();++i)
	CERR << "(" << g[G[i]]<< ")*(" << q[i] << ")+ ";
      CERR << rem << '\n';
    }
    if (env && env->moduloon){
      if (!rem.coord.empty() && rem.coord.front().g!=1)
	smallmult(invmod(rem.coord.front().g,env->modulo),rem.coord,rem.coord,env->modulo.val);
      return;
    }
    if (s!=1)
      smallmult(s,rem.coord,rem.coord);
    gen tmp=inplace_ppz(rem);
    if (debug_infolevel>1)
      CERR << "ppz was " << tmp << '\n';
  }

#endif // GBASIS_HEAP


  template<class tdeg_t>
  void smallmult(const gen & a,poly8<tdeg_t> & p,gen & m){
    typename std::vector< T_unsigned<gen,tdeg_t> >::iterator pt=p.coord.begin(),ptend=p.coord.end();
    if (a.type==_INT_ && m.type==_INT_){
      for (;pt!=ptend;++pt){
	if (pt->g.type==_INT_)
	  pt->g=(extend(pt->g.val)*a.val)%m.val;
	else
	  pt->g=smod(a*pt->g,m);
      }
    }
    else {
      for (;pt!=ptend;++pt){
	pt->g=smod(a*pt->g,m);
      }
    }
  }

  // p - a*q shifted mod m -> r
  template<class tdeg_t>
  void smallmultsub(const poly8<tdeg_t> & p,unsigned pos,int a,const poly8<tdeg_t> & q,const tdeg_t & shift,poly8<tdeg_t> & r,int m){
    r.coord.clear();
    r.coord.reserve(p.coord.size()+q.coord.size());
    typename vector< T_unsigned<gen,tdeg_t> >::const_iterator it=p.coord.begin()+pos,itend=p.coord.end(),jt=q.coord.begin(),jtend=q.coord.end();
    for (;jt!=jtend;++jt){
      tdeg_t v=jt->u+shift;
      for (;it!=itend && tdeg_t_strictly_greater(it->u,v,p.order);++it){
	r.coord.push_back(*it);
      }
      if (it!=itend && it->u==v){
	if (it->g.type==_INT_ && jt->g.type==_INT_){
	  int tmp=(it->g.val-extend(a)*jt->g.val)%m;
	  if (tmp)
	    r.coord.push_back(T_unsigned<gen,tdeg_t>(tmp,v));
	}
	else
	  r.coord.push_back(T_unsigned<gen,tdeg_t>(smod(it->g-a*jt->g,m),v));
	++it;
      }
      else {
	if (jt->g.type==_INT_){
	  int tmp=(-extend(a)*jt->g.val)%m;
	  r.coord.push_back(T_unsigned<gen,tdeg_t>(tmp,v));
	}
	else
	  r.coord.push_back(T_unsigned<gen,tdeg_t>(smod(-a*jt->g,m),v));
      }
    }
    for (;it!=itend;++it){
      r.coord.push_back(*it);
    }
  }

  // a and b are assumed to be _ZINT
  template<class tdeg_t>
  void linear_combination(const gen & a,const poly8<tdeg_t> &p,tdeg_t * ashift,const gen &b,const poly8<tdeg_t> & q,tdeg_t * bshift,poly8<tdeg_t> & r,environment * env){
    r.coord.clear();
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=q.coord.begin(),jtend=q.coord.end();
    r.coord.reserve((itend-it)+(jtend-jt));
    mpz_t tmpz;
    mpz_init(tmpz);
    if (jt!=jtend){
      tdeg_t v=jt->u;
      if (bshift)
	v=v+*bshift;
      for (;it!=itend;){
	tdeg_t u=it->u;
	if (ashift)
	  u=u+*ashift;
	if (u==v){
	  gen g;
#ifndef USE_GMP_REPLACEMENTS
	  if ( (it->g.type==_INT_ || it->g.type==_ZINT) &&
	       (jt->g.type==_INT_ || jt->g.type==_ZINT) ){
	    if (it->g.type==_INT_)
	      mpz_mul_si(tmpz,*a._ZINTptr,it->g.val);
	    else
	      mpz_mul(tmpz,*a._ZINTptr,*it->g._ZINTptr);
	    if (jt->g.type==_INT_){
	      if (jt->g.val>=0)
		mpz_addmul_ui(tmpz,*b._ZINTptr,jt->g.val);
	      else
		mpz_submul_ui(tmpz,*b._ZINTptr,-jt->g.val);
	    }
	    else
	      mpz_addmul(tmpz,*b._ZINTptr,*jt->g._ZINTptr);
	    if (mpz_sizeinbase(tmpz,2)<31)
	      g=int(mpz_get_si(tmpz));
	    else {
	      ref_mpz_t * ptr =new ref_mpz_t;
	      mpz_swap(ptr->z,tmpz);
	      g=ptr; // g=tmpz;
	    }
	  }
	  else
#endif
	    g=a*it->g+b*jt->g;
	  if (env && env->moduloon)
	    g=smod(g,env->modulo);
	  if (!is_zero(g))
	    r.coord.push_back(T_unsigned<gen,tdeg_t>(g,u));
	  ++it; ++jt;
	  if (jt==jtend)
	    break;
	  v=jt->u;
	  if (bshift)
	    v=v+*bshift;
	  continue;
	}
	if (tdeg_t_strictly_greater(u,v,p.order)){
	  gen g=a*it->g;
	  if (env && env->moduloon)
	    g=smod(g,env->modulo);
	  r.coord.push_back(T_unsigned<gen,tdeg_t>(g,u));
	  ++it;
	}
	else {
	  gen g=b*jt->g;
	  if (env && env->moduloon)
	    g=smod(g,env->modulo);
	  r.coord.push_back(T_unsigned<gen,tdeg_t>(g,v));
	  ++jt;
	  if (jt==jtend)
	    break;
	  v=jt->u;
	  if (bshift)
	    v=v+*bshift;
	}
      }
    }
    for (;it!=itend;++it){
      tdeg_t u=it->u;
      if (ashift)
	u=u+*ashift;
      gen g=a*it->g;
      if (env && env->moduloon)
	g=smod(g,env->modulo);
      r.coord.push_back(T_unsigned<gen,tdeg_t>(g,u));
    }
    for (;jt!=jtend;++jt){
      tdeg_t v=jt->u;
      if (bshift)
	v=v+*bshift;
      gen g=b*jt->g;
      if (env && env->moduloon)
	g=smod(g,env->modulo);
      r.coord.push_back(T_unsigned<gen,tdeg_t>(g,v));
    }
    mpz_clear(tmpz);
  }

  // check that &v1!=&v and &v2!=&v
  template<class tdeg_t>
  void sub(const poly8<tdeg_t> & v1,const poly8<tdeg_t> & v2,poly8<tdeg_t> & v,environment * env){
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator it1=v1.coord.begin(),it1end=v1.coord.end(),it2=v2.coord.begin(),it2end=v2.coord.end();
    gen g;
    v.coord.clear();
    v.coord.reserve((it1end-it1)+(it2end-it2)); // worst case
    for (;it1!=it1end && it2!=it2end;){
      if (it1->u==it2->u){
	g=it1->g-it2->g;
	if (env && env->moduloon)
	  g=smod(g,env->modulo);
	if (!is_zero(g))
	  v.coord.push_back(T_unsigned<gen,tdeg_t>(g,it1->u));
	++it1;
	++it2;
      }
      else {
	if (tdeg_t_strictly_greater(it1->u,it2->u,v1.order)){
	  v.coord.push_back(*it1);
	  ++it1;
	}
	else {
	  v.coord.push_back(T_unsigned<gen,tdeg_t>(-it2->g,it2->u));
	  ++it2;
	}
      }
    }
    for (;it1!=it1end;++it1)
      v.coord.push_back(*it1);
    for (;it2!=it2end;++it2)
      v.coord.push_back(T_unsigned<gen,tdeg_t>(-it2->g,it2->u));
  }
  
  template<class tdeg_t>
  void reduce(const poly8<tdeg_t> & p,const vectpoly8<tdeg_t> & res,const vector<unsigned> & G,unsigned excluded,vectpoly8<tdeg_t> & quo,poly8<tdeg_t> & rem,poly8<tdeg_t> & TMP1, poly8<tdeg_t> & TMP2,gen & lambda,environment * env,vector<bool> * Gusedptr=0){
    lambda=1;
    // last chance of improving = modular method for reduce or modular algo
    if (&p!=&rem)
      rem=p;
    if (p.coord.empty())
      return ;
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator pt,ptend;
    unsigned i,rempos=0;
    bool small0=env && env->moduloon && env->modulo.type==_INT_ && env->modulo.val;
    TMP1.order=p.order; TMP1.dim=p.dim; TMP2.order=p.order; TMP2.dim=p.dim; TMP1.coord.clear();
    for (unsigned count=0;;++count){
      ptend=rem.coord.end();
      // this branch search first in all leading coeff of G for a monomial 
      // <= to the current rem monomial
      pt=rem.coord.begin()+rempos;
      if (pt>=ptend)
	break;
      for (i=0;i<G.size();++i){
	if (i==excluded || res[G[i]].coord.empty())
	  continue;
	if (tdeg_t_all_greater(pt->u,res[G[i]].coord.front().u,p.order))
	  break;
      }
      if (i==G.size()){ // no leading coeff of G is smaller than the current coeff of rem
	++rempos;
	// if (small0) TMP1.coord.push_back(*pt);
	continue;
      }
      if (Gusedptr)
	(*Gusedptr)[i]=true;
      gen a(pt->g),b(res[G[i]].coord.front().g);
      if (small0){
	smallmultsub(rem,0,smod(a*invmod(b,env->modulo),env->modulo).val,res[G[i]],pt->u-res[G[i]].coord.front().u,TMP2,env->modulo.val);
	// smallmultsub(rem,rempos,smod(a*invmod(b,env->modulo),env->modulo).val,res[G[i]],pt->u-res[G[i]].coord.front().u,TMP2,env->modulo.val);
	// rempos=0; // since we have removed the beginning of rem (copied in TMP1)
	swap(rem.coord,TMP2.coord);
	continue;
      }
      TMP1.coord.clear();
      TMP2.coord.clear();
      tdeg_t resshift=pt->u-res[G[i]].coord.front().u;
      if (env && env->moduloon){
	gen ab=a;
	if (b!=1)
	  ab=a*invmod(b,env->modulo);
	ab=smod(ab,env->modulo);
	smallshift(res[G[i]].coord,resshift,TMP1.coord);
	if (ab!=1)
	  smallmult(ab,TMP1,env->modulo); 
	sub(rem,TMP1,TMP2,env);
      }
      else {
	// -b*rem+a*shift(res[G[i]])
	simplify(a,b);
	if (b==-1){
	  b=-b;
	  a=-a;
	}
	gen c=-b;
	if (a.type==_ZINT && c.type==_ZINT && !is_one(a) && !is_one(b)){
	  linear_combination<tdeg_t>(c,rem,0,a,res[G[i]],&resshift,TMP2,0);
	  lambda=c*lambda;
	}
	else {
	  smallshift(res[G[i]].coord,resshift,TMP1.coord);
	  if (!is_one(a))
	    inplace_mult(a,TMP1.coord);
	  if (!is_one(b)){
	    inplace_mult(b,rem.coord);
	    lambda=b*lambda;
	  }
	  sub(rem,TMP1,TMP2,0); 
	}
	//if (count % 6==5) inplace_ppz(TMP2,true,true); // quick gcd check
      }
      swap(rem.coord,TMP2.coord);
    }
    if (env && env->moduloon){
      // if (small0) swap(rem.coord,TMP1.coord);
      if (!rem.coord.empty() && rem.coord.front().g!=1)
	smallmult(invmod(rem.coord.front().g,env->modulo),rem.coord,rem.coord,env->modulo.val);
      return;
    }    
    gen g=inplace_ppz(rem);
    lambda=lambda/g;
    if (debug_infolevel>2){
      if (rem.coord.empty())
	CERR << "0 reduction" << '\n';
      if (g.type==_ZINT && mpz_sizeinbase(*g._ZINTptr,2)>16)
	CERR << "ppz size was " << mpz_sizeinbase(*g._ZINTptr,2) << '\n';
    }
  }

  template<class tdeg_t>
  void reduce(const poly8<tdeg_t> & p,const vectpoly8<tdeg_t> & res,const vector<unsigned> & G,unsigned excluded,vectpoly8<tdeg_t> & quo,poly8<tdeg_t> & rem,poly8<tdeg_t> & TMP1, poly8<tdeg_t> & TMP2,environment * env,vector<bool> * Gusedptr=0){
    gen lambda;
    reduce(p,res,G,excluded,quo,rem,TMP1,TMP2,lambda,env,Gusedptr);
  }

  template<class tdeg_t>
  void reduce1small(poly8<tdeg_t> & p,const poly8<tdeg_t> & q,poly8<tdeg_t> & TMP1, poly8<tdeg_t> & TMP2,environment * env){
    if (p.coord.empty())
      return ;
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator pt,ptend;
    unsigned rempos=0;
    TMP1.coord.clear();
    const tdeg_t & u = q.coord.front().u;
    const gen g=q.coord.front().g;
    for (unsigned count=0;;++count){
      ptend=p.coord.end();
      // this branch search first in all leading coeff of G for a monomial 
      // <= to the current rem monomial
      pt=p.coord.begin()+rempos;
      if (pt>=ptend)
	break;
      if (!tdeg_t_all_greater(pt->u,u,p.order)){
	++rempos;
	// TMP1.coord.push_back(*pt);
	continue;
      }
      smallmultsub(p,0,smod(pt->g*invmod(g,env->modulo),env->modulo).val,q,pt->u-u,TMP2,env->modulo.val);
      // smallmultsub(p,rempos,smod(a*invmod(b,env->modulo),env->modulo).val,q,pt->u-u,TMP2,env->modulo.val);
      // rempos=0; // since we have removed the beginning of rem (copied in TMP1)
      swap(p.coord,TMP2.coord);
    }
    // if (small0) swap(p.coord,TMP1.coord);
    if (env && env->moduloon && !p.coord.empty() && p.coord.front().g!=1)
      smallmult(invmod(p.coord.front().g,env->modulo),p.coord,p.coord,env->modulo.val);
  }

  // reduce with respect to itself the elements of res with index in G
  template<class tdeg_t>
  void reduce(vectpoly8<tdeg_t> & res,vector<unsigned> G,environment * env){
    if (res.empty() || G.empty())
      return;
    poly8<tdeg_t> pred(res.front().order,res.front().dim),
      TMP1(res.front().order,res.front().dim),
      TMP2(res.front().order,res.front().dim);
    vectpoly8<tdeg_t> q;
    // reduce res
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      poly8<tdeg_t> & p=res[i];
      reduce(p,res,G,i,q,pred,TMP1,TMP2,env);
      swap(res[i].coord,pred.coord);
      pred.sugar=res[i].sugar;
    }
  }

  template<class tdeg_t>
  void spoly(const poly8<tdeg_t> & p,const poly8<tdeg_t> & q,poly8<tdeg_t> & res,poly8<tdeg_t> & TMP1, environment * env){
    if (p.coord.empty()){
      res=q;
      return ;
    }
    if (q.coord.empty()){
      res= p;
      return;
    }
    const tdeg_t & pi = p.coord.front().u;
    const tdeg_t & qi = q.coord.front().u;
    tdeg_t lcm;
    index_lcm(pi,qi,lcm,p.order);
    tdeg_t pshift=lcm-pi;
    unsigned sugarshift=pshift.total_degree(p.order);
    // adjust sugar for res
    res.sugar=p.sugar+sugarshift;
    // CERR << "spoly " << res.sugar << " " << pi << qi << '\n';
    gen a=p.coord.front().g,b=q.coord.front().g;
    simplify3(a,b);
    if (debug_infolevel>2)
      CERR << "spoly " << a << " " << b << '\n';
    if (a.type==_ZINT && b.type==_ZINT){
      tdeg_t u=lcm-pi,v=lcm-qi;
      linear_combination(b,p,&u,a,q,&v,res,env);
    }
    else {
      poly8<tdeg_t> tmp1(p),tmp2(q);
      smallshift(tmp1.coord,lcm-pi,tmp1.coord);
      smallmult(b,tmp1.coord,tmp1.coord);
      smallshift(tmp2.coord,lcm-qi,tmp2.coord);
      smallmult(a,tmp2.coord,tmp2.coord);
      sub(tmp1,tmp2,res,env);
    }
    a=inplace_ppz(res);
    if (debug_infolevel>2)
      CERR << "spoly ppz " << a << '\n';
  }

  template<class tdeg_t>
  void gbasis_update(vector<unsigned> & G,vector< paire > & B,vectpoly8<tdeg_t> & res,unsigned pos,poly8<tdeg_t> & TMP1,poly8<tdeg_t> & TMP2,vectpoly8<tdeg_t> & vtmp,environment * env){
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin gbasis update " << '\n';
    const poly8<tdeg_t> & h = res[pos];
    order_t order=h.order;
    vector<unsigned> C;
    C.reserve(G.size());
    const tdeg_t & h0=h.coord.front().u;
    tdeg_t tmp1,tmp2;
    // C is used to construct new pairs
    // create pairs with h and elements g of G, then remove
    // -> if g leading monomial is prime with h, remove the pair
    // -> if g leading monomial is not disjoint from h leading monomial
    //    keep it only if lcm of leading monomial is not divisible by another one
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (res[G[i]].coord.empty() || disjoint(h0,res[G[i]].coord.front().u,res.front().order,res.front().dim))
	continue;
      index_lcm(h0,res[G[i]].coord.front().u,tmp1,order); // h0 and G[i] leading monomial not prime together
      unsigned j;
      for (j=0;j<G.size();++j){
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return;
	if (i==j || res[G[j]].coord.empty())
	  continue;
	index_lcm(h0,res[G[j]].coord.front().u,tmp2,order);
	if (tdeg_t_all_greater(tmp1,tmp2,order)){
	  // found another pair, keep the smallest, or the first if equal
	  if (tmp1!=tmp2)
	    break; 
	  if (i>j)
	    break;
	}
      } // end for j
      if (j==G.size())
	C.push_back(G[i]);
    }
    vector< paire > B1;
    B1.reserve(B.size()+C.size());
    for (unsigned i=0;i<B.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (res[B[i].first].coord.empty() || res[B[i].second].coord.empty())
	continue;
      index_lcm(res[B[i].first].coord.front().u,res[B[i].second].coord.front().u,tmp1,order);
      if (!tdeg_t_all_greater(tmp1,h0,order)){
	B1.push_back(B[i]);
	continue;
      }
      index_lcm(res[B[i].first].coord.front().u,h0,tmp2,order);
      if (tmp2==tmp1){
	B1.push_back(B[i]);
	continue;
      }
      index_lcm(res[B[i].second].coord.front().u,h0,tmp2,order);
      if (tmp2==tmp1){
	B1.push_back(B[i]);
	continue;
      }
    }
    // B <- B union pairs(h,g) with g in C
    for (unsigned i=0;i<C.size();++i)
      B1.push_back(paire(pos,C[i]));
    swap(B1,B);
    // Update G by removing elements with leading monomial >= leading monomial of h
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin Groebner interreduce " << '\n';
    C.clear();
    C.reserve(G.size());
    vector<unsigned> hG(1,pos);
    bool small0=env && env->moduloon && env->modulo.type==_INT_ && env->modulo.val;
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (!res[G[i]].coord.empty() && !tdeg_t_all_greater(res[G[i]].coord.front().u,h0,order)){
	// reduce res[G[i]] with respect to h
	if (small0)
	  reduce1small(res[G[i]],h,TMP1,TMP2,env);
	else
	  reduce(res[G[i]],res,hG,-1,vtmp,res[G[i]],TMP1,TMP2,env);
	C.push_back(G[i]);
      }
      // NB: removing all pairs containing i in it does not work
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " end Groebner interreduce " << '\n';
    C.push_back(pos);
    swap(C,G);
  }

  template<class tdeg_t>
  bool in_gbasis(vectpoly8<tdeg_t> & res,vector<unsigned> & G,environment * env,bool sugar){
    poly8<tdeg_t> TMP1(res.front().order,res.front().dim),TMP2(res.front().order,res.front().dim);
    vectpoly8<tdeg_t> vtmp;
    vector< paire > B;
    order_t order=res.front().order;
    //if (order==_PLEX_ORDER)
      sugar=false; // otherwise cyclic6 fails (bus error), don't know why
    for (unsigned l=0;l<res.size();++l){
      gbasis_update(G,B,res,l,TMP1,TMP2,vtmp,env);
    }
    for (int age=1;!B.empty() && !interrupted && !ctrl_c;++age){
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " number of pairs: " << B.size() << ", base size: " << G.size() << '\n';
      // find smallest lcm pair in B
      tdeg_t small0,cur;
      unsigned smallpos,smallsugar=0,cursugar=0;
      for (smallpos=0;smallpos<B.size();++smallpos){
	if (!res[B[smallpos].first].coord.empty() && !res[B[smallpos].second].coord.empty())
	  break;
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return false;
      }
      index_lcm(res[B[smallpos].first].coord.front().u,res[B[smallpos].second].coord.front().u,small0,order);
      if (sugar)
	smallsugar=res[B[smallpos].first].sugar+(small0-res[B[smallpos].first].coord.front().u).total_degree(order);
      for (unsigned i=smallpos+1;i<B.size();++i){
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return false;
	if (res[B[i].first].coord.empty() || res[B[i].second].coord.empty())
	  continue;
	index_lcm(res[B[i].first].coord.front().u,res[B[i].second].coord.front().u,cur,order);
	if (sugar)
	  cursugar=res[B[smallpos].first].sugar+(cur-res[B[smallpos].first].coord.front().u).total_degree(order);
	bool doswap;
	if (order.o==_PLEX_ORDER)
	  doswap=tdeg_t_strictly_greater(small0,cur,order);
	else {
	  if (cursugar!=smallsugar)
	    doswap = smallsugar > cursugar;
	  else
	    doswap=tdeg_t_strictly_greater(small0,cur,order);
	}
	if (doswap){
	  // CERR << "swap " << cursugar << " " << res[B[i].first].coord.front().u << " " << res[B[i].second].coord.front().u << '\n';
	  swap(small0,cur); // small0=cur;
	  swap(smallsugar,cursugar);
	  smallpos=i;
	}
      }
      paire bk=B[smallpos];
      if (debug_infolevel>1 && (equalposcomp(G,bk.first)==0 || equalposcomp(G,bk.second)==0))
	CERR << CLOCK()*1e-6 << " reducing pair with 1 element not in basis " << bk << '\n';
      B.erase(B.begin()+smallpos);
      poly8<tdeg_t> h(res.front().order,res.front().dim);
      spoly(res[bk.first],res[bk.second],h,TMP1,env);
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " reduce begin, pair " << bk << " remainder size " << h.coord.size() << '\n';
      reduce(h,res,G,-1,vtmp,h,TMP1,TMP2,env);
      if (debug_infolevel>1){
	if (debug_infolevel>3){ CERR << h << '\n'; }
	CERR << CLOCK()*1e-6 << " reduce end, remainder size " << h.coord.size() << '\n';
      }
      if (!h.coord.empty()){
	res.push_back(h);
	gbasis_update(G,B,res,unsigned(res.size()-1),TMP1,TMP2,vtmp,env);
	if (debug_infolevel>2)
	  CERR << CLOCK()*1e-6 << " basis indexes " << G << " pairs indexes " << B << '\n';
      }
    }
    return true;
  }

  longlong invmod(longlong a,longlong b){
    if (a==1 || a==-1 || a==1-b)
      return a;
    longlong aa(1),ab(0),ar(0);
#ifdef VISUALC
    longlong q,r;
    while (b){
      q=a/b;
      r=a-q*b;
      ar=aa-q*ab;
      a=b;
      b=r;
      aa=ab;
      ab=ar;
    }
#else
    lldiv_t qr;
    while (b){
      qr=lldiv(a,b);
      ar=aa-qr.quot*ab;
      a=b;
      b=qr.rem;
      aa=ab;
      ab=ar;
    }
#endif
    if (a==1)
      return aa;
    if (a!=-1){
#ifndef NO_STDEXCEPT
      setsizeerr(gettext("Not invertible"));
#endif
      return 0;
    }
    return -aa;
  }

  longlong smod(longlong a,longlong b){
    longlong r=a%b;
    if (r>b/2)
      r -= b;
    else {
      if (r<=-b/2)
	r += b;
    }
    return r;
  }

#ifdef x86_64
  // typedef longlong modint;
  // typedef int128_t modint2;
  longlong smod(int128_t a,longlong b){
    longlong r=a%b;
    if (r>b/2)
      r -= b;
    else {
      if (r<=-b/2)
	r += b;
    }
    return r;
  }
#endif


  template<class tdeg_t,class modint_t>
  struct polymod {
    std::vector< T_unsigned<modint_t,tdeg_t> > coord;
    // lex order is implemented using tdeg_t as a list of degrees
    // tdeg uses total degree 1st then partial degree in lex order, max 7 vars
    // revlex uses total degree 1st then opposite of partial degree in reverse ordre, max 7 vars
    order_t order; // _PLEX_ORDER, _REVLEX_ORDER or _TDEG_ORDER or _7VAR_ORDER or _11VAR_ORDER
    short int dim;
    unsigned sugar;
    int fromleft,fromright,age;
    double logz; // trace origin as a s-polynomial
    void dbgprint() const;
    void swap(polymod & q){ 
      order_t tmp;
      tmp=order; order=q.order; q.order=tmp;
      int tmp2=dim; dim=q.dim; q.dim=tmp2;
      tmp2=sugar; sugar=q.sugar; q.sugar=tmp2;
      coord.swap(q.coord);
      tmp2=fromleft; fromleft=q.fromleft; q.fromleft=tmp2;
      tmp2=fromright; fromright=q.fromright; q.fromright=tmp2;
      tmp2=age; age=q.age; q.age=tmp2; 
      double tmp3=logz; logz=q.logz; q.logz=tmp3; 
   }
    polymod():dim(0),fromleft(-1),fromright(-1),logz(1) {order_t tmp={_PLEX_ORDER,0}; order=tmp;}
    polymod(order_t o_,int dim_): dim(dim_),fromleft(-1),fromright(-1),logz(1) {order=o_; order.dim=dim_;}
    polymod(const polynome & p,order_t o_,modint_t m):fromleft(-1),fromright(-1),logz(1){
      order=o_; 
      dim=p.dim;
      order.dim=dim;
      if (order.o%4!=3){
	if (p.is_strictly_greater==i_lex_is_strictly_greater)
	  order.o=_PLEX_ORDER;
	if (p.is_strictly_greater==i_total_revlex_is_strictly_greater)
	  order.o=_REVLEX_ORDER;
	if (p.is_strictly_greater==i_total_lex_is_strictly_greater)
	  order.o=_TDEG_ORDER;
      }
      if (p.dim>GROEBNER_VARS-(order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER)) 
	CERR << "Number of variables is too large to be handled by giac";
      else {
	if (!p.coord.empty()){
	  coord.reserve(p.coord.size());
	  for (unsigned i=0;i<p.coord.size();++i){
	    modint_t n;
	    if (p.coord[i].value.type==_ZINT)
	      n=modulo(*p.coord[i].value._ZINTptr,m);
	    else
	      n=p.coord[i].value.val % m;
	    coord.push_back(T_unsigned<modint_t,tdeg_t>(n,tdeg_t(p.coord[i].index,order)));
	  }
	  sugar=coord.front().u.total_degree(order);
	}
      }
    }
    void get_polynome(polynome & p) const {
      p.dim=dim;
      switch (order.o){
      case _PLEX_ORDER:
	p.is_strictly_greater=i_lex_is_strictly_greater;
	break;
      case _REVLEX_ORDER:
	p.is_strictly_greater=i_total_revlex_is_strictly_greater;
	break;
      case _3VAR_ORDER:
	p.is_strictly_greater=i_3var_is_strictly_greater;
	break;
      case _7VAR_ORDER:
	p.is_strictly_greater=i_7var_is_strictly_greater;
	break;
      case _11VAR_ORDER:
	p.is_strictly_greater=i_11var_is_strictly_greater;
	break;
      case _TDEG_ORDER:
	p.is_strictly_greater=i_total_lex_is_strictly_greater;
	break;
      }
      p.coord.clear();
      p.coord.reserve(coord.size());
      index_t idx(dim);
      for (unsigned i=0;i<coord.size();++i){
	get_index(coord[i].u,idx,order,dim);
	p.coord.push_back(monomial<gen>(coord[i].g,idx));
      }
      // if (order==_3VAR_ORDER || order==_7VAR_ORDER || order==_11VAR_ORDER) p.tsort();
    }
  }; // end polymod

  template<class tdeg_t> void convert(const polymod<tdeg_t,modint> & src, polymod<tdeg_t,mod4int> & target){
    typename std::vector< T_unsigned<modint,tdeg_t> >::const_iterator it=src.coord.begin(),itend=src.coord.end();
    target.coord.clear(); target.coord.reserve(itend-it);
    for (;it!=itend;++it){
      mod4int m4={it->g,it->g,it->g,it->g};
      target.coord.push_back(T_unsigned<mod4int,tdeg_t>(m4,it->u));
    }
  }

  template<class tdeg_t> void convert(const polymod<tdeg_t,modint> & src, polymod<tdeg_t,modint> & target,int pos){
    target=src;
  }
  template<class tdeg_t> void convert(const polymod<tdeg_t,mod4int> & src, polymod<tdeg_t,modint> & target,int pos){
    target.dim=src.dim; target.order=src.order; target.sugar=src.sugar; target.fromleft=src.fromleft; target.fromright=src.fromright; target.age=src.age; target.logz=src.logz;
    typename std::vector< T_unsigned<mod4int,tdeg_t> >::const_iterator it=src.coord.begin(),itend=src.coord.end();
    target.coord.clear(); target.coord.reserve(itend-it);
    for (;it!=itend;++it){
      if (it->g.tab[pos]!=0) target.coord.push_back(T_unsigned<modint,tdeg_t>(it->g.tab[pos],it->u));
    }
  }

  template<class T>
  void increase(vector<T> &v){
    if (v.size()!=v.capacity())
      return;
    vector<T> w;
    w.reserve(v.size()*2);
    for (unsigned i=0;i<v.size();++i){
      w.push_back(T(v[i].order,v[i].dim));
      w[i].coord.swap(v[i].coord);
    }
    v.swap(w);
  }
  
  template<class tdeg_t,class modint_t>
  struct polymod_sort_t {
    polymod_sort_t() {}
    bool operator () (const polymod<tdeg_t,modint_t> & p,const polymod<tdeg_t,modint_t> & q) const {
      if (q.coord.empty())
	return false;
      if (p.coord.empty())
	return true;
      if (p.coord.front().u==q.coord.front().u)
	return false;
      return tdeg_t_greater(q.coord.front().u,p.coord.front().u,p.order); // p.coord.front().u<q.coord.front().u; 
      // this should be enough to sort groebner basis
    }
  };

  template<class modint_t>
  inline modint_t makepositive(modint_t a,modint_t n){
    return a+((a>>31)&n);
    // return a-(a>>31)*n; // return a<0?a+n:a;
  }

  template<class tdeg_t,class modint_t>
  void smallmultmod(modint_t a,polymod<tdeg_t,modint_t> & p,modint_t m,bool mkpositive=true){
#if 1 // ndef GBASIS_4PRIMES
    if (a==1 || a==create<modint_t>(1)-m)
      return;
#endif
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::iterator pt=p.coord.begin(),ptend=p.coord.end();
    if (mkpositive){
      for (;pt!=ptend;++pt){
	modint_t tmp=(extend(pt->g)*a)%m;
	pt->g=makepositive(tmp,m); // if (tmp<0) tmp += m; pt->g=tmp;
      }
    }
    else {
      for (;pt!=ptend;++pt){
	modint_t tmp=(extend(pt->g)*a)%m;
	pt->g=tmp;
      }
    }
  }

  template<class tdeg_t>
  struct tdeg_t_sort_t {
    order_t order;
    tdeg_t_sort_t() {order_t tmp={_REVLEX_ORDER,0}; order=tmp;}
    tdeg_t_sort_t(order_t o):order(o) {}
    bool operator ()(const T_unsigned<modint,tdeg_t> & a,const T_unsigned<modint,tdeg_t> & b) const {return !tdeg_t_greater(b.u,a.u,order);}
    bool operator ()(const T_unsigned<mod4int,tdeg_t> & a,const T_unsigned<mod4int,tdeg_t> & b) const {return !tdeg_t_greater(b.u,a.u,order);}
    bool operator ()(const T_unsigned<gen,tdeg_t> & a,const T_unsigned<gen,tdeg_t> & b) const {return !tdeg_t_greater(b.u,a.u,order);}
    bool operator ()(const tdeg_t & a,const tdeg_t & b) const {return !tdeg_t_greater(b,a,order);}
    bool operator ()(const pair<int,tdeg_t> & a,const pair<int,tdeg_t> & b) const {return !tdeg_t_greater(b.second,a.second,order);}
  };

  template<class tdeg_t,class modint_t>
  void convert(const poly8<tdeg_t> & p,polymod<tdeg_t,modint_t> &q,modint_t env,bool unitarize=true){
#if 0
    q.coord.reserve(p.coord.size());
    q.dim=p.dim;
    q.order=p.order;
    q.sugar=0;
    for (unsigned i=0;i<p.coord.size();++i){
      int g=1;
      if (env){
	if (p.coord[i].g.type==_ZINT)
	  g=modulo(*p.coord[i].g._ZINTptr,env);
	else
	  g=(p.coord[i].g.val)%env;
      }
      if (g!=0)
	q.coord.push_back(T_unsigned<int,tdeg_t>(g,p.coord[i].u));
    }
#else
    q.coord.reserve(p.coord.size());
    q.dim=p.dim;
    q.order=p.order;
    q.age=q.sugar=0;
    for (unsigned i=0;i<p.coord.size();++i){
      modint_t g;
      if (is_zero(env))
	g=create<modint_t>(1);
      else {
	if (p.coord[i].g.type==_ZINT)
	  g=modulo(*p.coord[i].g._ZINTptr,env);
	else
	  g=(p.coord[i].g.val)%env;
      }
      if (!is_zero(g))
        q.coord.push_back(T_unsigned<modint_t,tdeg_t>(g,p.coord[i].u));
    }
#endif
    if (!is_zero(env) && unitarize && !q.coord.empty()){
      q.sugar=q.coord.front().u.total_degree(p.order);
#if 1 // ndef GBASIS_4PRIMES
      if (q.coord.front().g!=1)
#endif
	smallmultmod(invmod(q.coord.front().g,env),q,env);
      q.coord.front().g=create<modint_t>(1);
    }
    sort(q.coord.begin(),q.coord.end(),tdeg_t_sort_t<tdeg_t>(p.order));
  }
  template<class tdeg_t,class modint_t>
  void convert(const polymod<tdeg_t,modint_t> & p,poly8<tdeg_t> &q,modint_t env){
    q.coord.resize(p.coord.size());
    q.dim=p.dim;
    q.order=p.order;
    for (unsigned i=0;i<p.coord.size();++i){
      modint_t n=smod(p.coord[i].g, env);
      q.coord[i].g=n;
      q.coord[i].u=p.coord[i].u;
    }
    if (!q.coord.empty())
      q.sugar=q.coord.front().u.total_degree(p.order);
    else
      q.sugar=0;
  }

  template<class tdeg_t,class modint_t>
  bool operator == (const polymod<tdeg_t,modint_t> & p,const polymod<tdeg_t,modint_t> &q){
    if (p.coord.size()!=q.coord.size())
      return false;
    for (unsigned i=0;i<p.coord.size();++i){
      if (p.coord[i].u!=q.coord[i].u || p.coord[i].g!=q.coord[i].g)
	return false;
    }
    return true;
  }

#ifdef NSPIRE
  template<class T,class tdeg_t,class modint_t>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os, const polymod<tdeg_t,modint_t> & p)
#else
  template<class tdeg_t,class modint_t>
  ostream & operator << (ostream & os, const polymod<tdeg_t,modint_t> & p)
#endif
  {
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    int t2;
    if (it==itend)
      return os << 0 ;
    for (;it!=itend;){
      os << it->g  ;
#ifndef GBASIS_NO_OUTPUT
      if (it->u.vars64()){
	if (it->u.tdeg%2){
	  degtype * i=(degtype *)(it->u.ui+1);
	  for (int j=0;j<it->u.order_.dim;++j){
	    t2=i[j];
	    if (t2)
	      os << "*x"<< j << "^" << t2  ;
	  }
	  ++it;
	  if (it==itend)
	    break;
	  os << " + ";
	  continue;
	}
      }
#endif
      short tab[GROEBNER_VARS+1];
      tab[GROEBNER_VARS]=0;
      it->u.get_tab(tab,p.order);
      switch (p.order.o){
      case _PLEX_ORDER:
	for (int i=0;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2)
	    os << "*x"<< i << "^" << t2  ;
	}
	break;
      case _TDEG_ORDER:
	for (int i=1;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  if (t2)
	    os << "*x"<< i-1 << "^" << t2  ;
	}
	break;
      case _REVLEX_ORDER:
	for (int i=1;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;
#if GROEBNER_VARS==15
      case _3VAR_ORDER:
	for (int i=1;i<=3;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 3-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=5;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 7+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
      case _7VAR_ORDER:
	for (int i=1;i<=7;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 7-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=9;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 11+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
      case _11VAR_ORDER:
	for (int i=1;i<=11;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 11-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=13;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 15+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
#endif
      }
      ++it;
      if (it==itend)
	break;
      os << " + ";
    }
    return os;
  }

  template<class tdeg_t,class modint_t>
  void polymod<tdeg_t,modint_t>::dbgprint() const { 
    CERR << *this << '\n';
  }

  template<class tdeg_t,class modint_t>
  class vectpolymod:public vector< polymod<tdeg_t,modint_t> >{
  public:
    void dbgprint() const { CERR << *this << '\n'; }
  };

  template<class tdeg_t,class modint_t> void convert(const vectpolymod<tdeg_t,modint_t> & src, vectpolymod<tdeg_t,modint> & target,int pos){
    target.resize(src.size());
    for (int i=0;i<src.size();++i)
      convert(src[i],target[i],pos);
  }


  template<class tdeg_t,class modint_t>
  void vectpoly_2_vectpolymod(const vectpoly & v,order_t order,vectpolymod<tdeg_t,modint_t> & v8,modint_t m){
    v8.clear();
    v8.reserve(v.size());
    for (unsigned i=0;i<v.size();++i){
      v8.push_back(polymod<tdeg_t,modint_t>(v[i],order,m));
      v8.back().order=order;
    }
  }

  template<class tdeg_t,class modint_t>
  void convert(const vectpoly8<tdeg_t> & v,vectpolymod<tdeg_t,modint_t> & w,modint_t env,int n=0,bool unitarize=true){
    if (n==0)
      n=v.size();
    if (w.size()<n)
      w.resize(n);
    for (unsigned i=0;i<n;++i){
      convert(v[i],w[i],env,unitarize);
    }
  }

  template<class tdeg_t,class modint_t>
  void convert(const vectpolymod<tdeg_t,modint_t> & v,vectpoly8<tdeg_t> & w,modint_t env){
    w.resize(v.size());
    for (unsigned i=0;i<v.size();++i){
      convert(v[i],w[i],env);
    }
  }

  template<class tdeg_t,class modint_t>
  void convert(const vectpolymod<tdeg_t,modint_t> & v,const vector<unsigned> & G,vectpoly8<tdeg_t> & w,modint_t env){
    w.resize(v.size());
    for (unsigned i=0;i<G.size();++i){
      convert(v[G[i]],w[G[i]],env);
    }
  }

#ifdef GBASIS_HEAP
  template<class tdeg_t,class modint_t>
  void in_heap_reducemod(const polymod<tdeg_t,modint_t> & f,const vectpolymod<tdeg_t,modint_t> & g,const vector<unsigned> & G,unsigned excluded,vectpolymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & rem,polymod<tdeg_t,modint_t> * R,modint env){
    // divides f by g[G[0]] to g[G[G.size()-1]] except maybe g[G[excluded]]
    // first implementation: use quotxsient heap for all quotient/divisor
    // do not use heap chain
    // ref Monaghan Pearce if g.size()==1
    // R is the list of all monomials
    if (R){
      R->dim=f.dim; R->order=f.order;
      R->coord.clear();
    }
    if (&rem==&f){
      polymod<tdeg_t,modint_t> TMP;
      in_heap_reducemod(f,g,G,excluded,q,TMP,R,env);
      swap(rem.coord,TMP.coord);
      if (debug_infolevel>1000)
	g.dbgprint(); // instantiate dbgprint()
      return;
    }
    rem.coord.clear();
    if (f.coord.empty())
      return ;
    if (q.size()<G.size())
      q.resize(G.size());
    unsigned guess=0;
    for (unsigned i=0;i<G.size();++i){
      q[i].dim=f.dim;
      q[i].order=f.order;
      q[i].coord.clear();
      guess += unsigned(g[G[i]].coord.size());
    }
    vector<heap_t<tdeg_t> > H;
    compare_heap_t<tdeg_t> key(f.order);
    H.reserve(guess);
    vector<modint_t> invlcg(G.size());
    for (unsigned i=0;i<G.size();++i){
      invlcg[i]=invmod(g[G[i]].coord.front().g,env);
    }
    modint_t c=1;
#ifdef x86_64
    int128_t C=0; // int128_t to avoid %
#else
    modint2 C=0; // int128_t to avoid %
#endif
    unsigned k=0,i; // k=position in f
    tdeg_t m;
    bool finish=false;
    while (!H.empty() || k<f.coord.size()){
      // is highest remaining degree in f or heap?
      if (k<f.coord.size() && (H.empty() || tdeg_t_greater(f.coord[k].u,H.front().u,f.order)) ){
	// it's in f or both
	m=f.coord[k].u;
	C=f.coord[k].g;
	++k;
      }
      else {
	m=H[0].u;
	C=0;
      }
      if (R)
	R->coord.push_back(T_unsigned<modint_t,tdeg_t>(1,m));
      // extract from heap all terms having m as monomials, subtract from c
      while (!H.empty() && H.front().u==m){
	std::pop_heap(H.begin(),H.end(),key);
	heap_t<tdeg_t> & current=H.back(); // was root node of the heap
	const polymod<tdeg_t,modint_t> & gcurrent = g[G[current.i]];
	if (!R){
#ifdef x86_64
	  C -= extend(q[current.i].coord[current.qi].g) * gcurrent.coord[current.gj].g;
#else
	  C = (C-extend(q[current.i].coord[current.qi].g) * gcurrent.coord[current.gj].g) % env;
#endif
	}
	if (current.gj<gcurrent.coord.size()-1){
	  ++current.gj;
	  current.u=q[current.i].coord[current.qi].u+gcurrent.coord[current.gj].u;
	  push_heap(H.begin(),H.end(),key);
	}
	else
	  H.pop_back();
      }
      if (!R){
#ifdef x86_64
	c = C % env;
#else
	c=shrink(C);
#endif
	if (c==0)
	  continue;
      }
      // divide (c,m) by one of the g if possible, otherwise push in remainder
      if (finish){
	rem.coord.push_back(T_unsigned<modint_t,tdeg_t>(c,m)); // add c*m to remainder
	continue;
      }
      finish=true;
#if 0
      for (i=G.size()-1;i!=-1;--i){
	if (i==excluded || g[G[i]].coord.empty())
	  continue;
	if (tdeg_t_greater(m,g[G[i]].coord.front().u,f.order)){
	  finish=false;
	  if (tdeg_t_all_greater(m,g[G[i]].coord.front().u,f.order))
	    break;
	}
      }
      if (i==-1){
	rem.coord.push_back(T_unsigned<modint_t,tdeg_t>(c,m)); // add c*m to remainder
	continue;
      }
#else
      for (i=0;i<G.size();++i){
	if (i==excluded || g[G[i]].coord.empty())
	  continue;
	if (tdeg_t_greater(m,g[G[i]].coord.front().u,f.order)){
	  finish=false;
	  if (tdeg_t_all_greater(m,g[G[i]].coord.front().u,f.order))
	    break;
	}
      }
      if (i==G.size()){
	rem.coord.push_back(T_unsigned<modint_t,tdeg_t>(c,m)); // add c*m to remainder
	continue;
      }
#endif
      // add c*m/leading monomial of g[G[i]] to q[i]
      tdeg_t monom=m-g[G[i]].coord.front().u;
      if (!R){
	if (invlcg[i]!=1){
	  if (invlcg[i]==-1)
	    c=-c;
	  else
	    c=(extend(c)*invlcg[i]) % env;
	}
      }
      q[i].coord.push_back(T_unsigned<modint_t,tdeg_t>(c,monom));
      // push in heap
      if (g[G[i]].coord.size()>1){
	heap_t<tdeg_t> current={i,unsigned(q[i].coord.size())-1,1,g[G[i]].coord[1].u+monom};
	H.push_back(current);
	push_heap(H.begin(),H.end(),key);
      }
    } // end main heap pseudo-division loop
  }

  template<class tdeg_t,class modint_t>
  void heap_reducemod(const polymod<tdeg_t,modint_t> & f,const vectpolymod<tdeg_t,modint_t> & g,const vector<unsigned> & G,unsigned excluded,vectpolymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & rem,modint_t env){
    in_heap_reducemod(f,g,G,excluded,q,rem,0,env);
    // end up by multiplying rem by s (so that everything is integer)
    if (debug_infolevel>2){
      for (unsigned i=0;i<G.size();++i)
	CERR << "(" << g[G[i]]<< ")*(" << q[i] << ")+ ";
      CERR << rem << '\n';
    }
    if (!rem.coord.empty() && rem.coord.front().g!=1){
      smallmult(invmod(rem.coord.front().g,env),rem.coord,rem.coord,env);
      rem.coord.front().g=1;
    }
  }

#endif

  template<class tdeg_t,class modint_t>
  void symbolic_preprocess(const polymod<tdeg_t,modint_t> & f,const vectpolymod<tdeg_t,modint_t> & g,const vector<unsigned> & G,unsigned excluded,vectpolymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & rem,polymod<tdeg_t,modint_t> * R){
    // divides f by g[G[0]] to g[G[G.size()-1]] except maybe g[G[excluded]]
    // first implementation: use quotient heap for all quotient/divisor
    // do not use heap chain
    // ref Monaghan Pearce if g.size()==1
    // R is the list of all monomials
    if (R){
      R->dim=f.dim; R->order=f.order;
      R->coord.clear();
    }
    rem.coord.clear();
    if (f.coord.empty())
      return ;
    if (q.size()<G.size())
      q.resize(G.size());
    unsigned guess=0;
    for (unsigned i=0;i<G.size();++i){
      q[i].dim=f.dim;
      q[i].order=f.order;
      q[i].coord.clear();
      guess += unsigned(g[G[i]].coord.size());
    }
    vector<heap_t<tdeg_t> > H_;
    vector<unsigned> H;
    H_.reserve(guess);
    H.reserve(guess);
    heap_t_compare<tdeg_t> keyheap(H_,f.order);
    unsigned k=0,i; // k=position in f
    tdeg_t m;
    bool finish=false;
    while (!H.empty() || k<f.coord.size()){
      // is highest remaining degree in f or heap?
      if (k<f.coord.size() && (H.empty() || tdeg_t_greater(f.coord[k].u,H_[H.front()].u,f.order)) ){
	// it's in f or both
	m=f.coord[k].u;
	++k;
      }
      else {
	m=H_[H.front()].u;
      }
      if (R)
	R->coord.push_back(T_unsigned<modint_t,tdeg_t>(1,m));
      // extract from heap all terms having m as monomials, subtract from c
      while (!H.empty() && H_[H.front()].u==m){
	std::pop_heap(H.begin(),H.end(),keyheap);
	heap_t<tdeg_t> & current=H_[H.back()]; // was root node of the heap
	const polymod<tdeg_t,modint_t> & gcurrent = g[G[current.i]];
	if (current.gj<gcurrent.coord.size()-1){
	  ++current.gj;
	  current.u=q[current.i].coord[current.qi].u+gcurrent.coord[current.gj].u;
	  std::push_heap(H.begin(),H.end(),keyheap);
	}
	else
	  H.pop_back();
      }
      // divide (c,m) by one of the g if possible, otherwise push in remainder
      if (finish){
	rem.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,m)); // add to remainder
	continue;
      }
      finish=true;
      for (i=0;i<G.size();++i){
	const vector< T_unsigned<modint_t,tdeg_t> > & gGicoord=g[G[i]].coord;
	if (i==excluded || gGicoord.empty())
	  continue;
	if (tdeg_t_greater(m,gGicoord.front().u,f.order)){
	  finish=false;
	  if (tdeg_t_all_greater(m,gGicoord.front().u,f.order))
	    break;
	}
      }
      if (i==G.size()){
	rem.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,m)); // add to remainder
	continue;
      }
      // add m/leading monomial of g[G[i]] to q[i]
      tdeg_t monom=m-g[G[i]].coord.front().u;
      q[i].coord.push_back(T_unsigned<modint_t,tdeg_t>(1,monom));
      // push in heap
      if (g[G[i]].coord.size()>1){
	heap_t<tdeg_t> current={i,unsigned(q[i].coord.size())-1,1,g[G[i]].coord[1].u+monom};
	H.push_back(unsigned(H_.size()));
	H_.push_back(current);
	keyheap.ptr=&H_.front();
	std::push_heap(H.begin(),H.end(),keyheap);
      }
    } // end main heap pseudo-division loop
    // CERR << H_.size() << '\n';
  }

  // p - a*q shifted mod m -> r
  template<class tdeg_t,class modint_t>
  void smallmultsubmodshift(const polymod<tdeg_t,modint_t> & p,unsigned pos,modint_t a,const polymod<tdeg_t,modint_t> & q,const tdeg_t & shift,polymod<tdeg_t,modint_t> & r,modint_t m){
    r.coord.clear();
    r.coord.reserve(p.coord.size()+q.coord.size());
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it0=p.coord.begin(),it=it0+pos,itend=p.coord.end(),jt=q.coord.begin(),jtend=q.coord.end();
    // for (;it0!=it;++it0){ r.coord.push_back(*it0); }
    tdeg_t v=shift+shift; // new memory slot
    int dim=p.dim;
    for (;jt!=jtend;++jt){
      //CERR << "dbg " << jt->u << " " << shift << "\n";
      add(jt->u,shift,v,dim);
      for (;it!=itend && tdeg_t_strictly_greater(it->u,v,p.order);++it){
	r.coord.push_back(*it);
      }
      if (it!=itend && it->u==v){
	modint_t tmp=(it->g-extend(a)*jt->g)%m;
	if (!is_zero(tmp))
	  r.coord.push_back(T_unsigned<modint_t,tdeg_t>(tmp,v));
	++it;
      }
      else {
	modint_t tmp=(-extend(a)*jt->g)%m;
	r.coord.push_back(T_unsigned<modint_t,tdeg_t>(tmp,v));
      }
    }
    for (;it!=itend;++it){
      r.coord.push_back(*it);
    }
  }

#ifdef HASH_MAP_NAMESPACE
  // p -= a*q shifted mod m -> r
  template<class tdeg_t,class modint_t>
  void mapmultsubmodshift(HASH_MAP_NAMESPACE::hash_map<tdeg_t,modint_t> & p,modint_t a,const polymod<tdeg_t,modint_t> & q,const tdeg_t & shift,modint_t m){
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=q.coord.begin(),jtend=q.coord.end();
    // for (;it0!=it;++it0){ r.coord.push_back(*it0); }
    tdeg_t v=shift+shift; // new memory slot
    int dim=q.dim;
    for (;jt!=jtend;++jt){
      //CERR << "dbg " << jt->u << " " << shift << "\n";
      add(jt->u,shift,v,dim);
      typename HASH_MAP_NAMESPACE::hash_map<tdeg_t,modint_t>::iterator it=p.find(v),itend=p.end();
      if (it!=itend){
	modint_t tmp=(it->second-extend(a)*jt->g)%m;
        it->second=tmp;
      }
      else {
	modint_t tmp=(-extend(a)*jt->g)%m;
        p[v]=tmp;
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void poly2map(const polymod<tdeg_t,modint_t> & p,HASH_MAP_NAMESPACE::hash_map<tdeg_t,modint_t> & m){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    for (;it!=itend;++it)
      m[it->u]=it->g;
  }

  template<class tdeg_t,class modint_t>
  void map2poly(const HASH_MAP_NAMESPACE::hash_map<tdeg_t,modint_t> & m,polymod<tdeg_t,modint_t> & p){
    p.coord.clear();
    typename HASH_MAP_NAMESPACE::hash_map<tdeg_t,modint_t>::const_iterator it=m.begin(),itend=m.end();
    for (;it!=itend;++it){
      if (!is_zero(it->second))
        p.coord.push_back(T_unsigned<modint_t,tdeg_t>(it->second,it->first));
    }
    sort(p.coord.begin(),p.coord.end(),tdeg_t_sort_t<tdeg_t>(p.order));
  }

#endif

  // p -= a*q shifted mod m -> r
  template<class tdeg_t,class modint_t>
  void mapmultsubmodshift(map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> > & p,modint_t a,const polymod<tdeg_t,modint_t> & q,const tdeg_t & shift,modint_t m){
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=q.coord.begin(),jtend=q.coord.end();
    // for (;it0!=it;++it0){ r.coord.push_back(*it0); }
    tdeg_t v=shift+shift; // new memory slot
    int dim=q.dim;
    for (;jt!=jtend;++jt){
      //CERR << "dbg " << jt->u << " " << shift << "\n";
      add(jt->u,shift,v,dim);
      typename map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> >::iterator it=p.find(v),itend=p.end();
      if (it!=itend){
	modint_t tmp=(it->second-extend(a)*jt->g)%m;
        it->second=tmp;
      }
      else {
	modint_t tmp=(-extend(a)*jt->g)%m;
        p[v]=tmp;
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void poly2map(const polymod<tdeg_t,modint_t> & p,map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> > & m){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    for (;it!=itend;++it)
      m[it->u]=it->g;
  }

  template<class tdeg_t,class modint_t>
  void map2poly(const map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> > & m,polymod<tdeg_t,modint_t> & p){
    p.coord.clear();
    typename map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> >::const_iterator it=m.begin(),itend=m.end();
    for (;it!=itend;++it){
      if (!is_zero(it->second))
        p.coord.push_back(T_unsigned<modint_t,tdeg_t>(it->second,it->first));
    }
  }

  // p - a*q mod m -> r
  template<class tdeg_t,class modint_t>
  void smallmultsubmod(const polymod<tdeg_t,modint_t> & p,modint_t a,const polymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & r,modint_t m){
    r.coord.clear();
    r.coord.reserve(p.coord.size()+q.coord.size());
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=q.coord.begin(),jtend=q.coord.end();
    for (;jt!=jtend;++jt){
      const tdeg_t & v=jt->u;
      for (;it!=itend && tdeg_t_strictly_greater(it->u,v,p.order);++it){
	r.coord.push_back(*it);
      }
      if (it!=itend && it->u==v){
	modint_t tmp=(it->g-extend(a)*jt->g)%m;
	if (!is_zero(tmp))
	  r.coord.push_back(T_unsigned<modint_t,tdeg_t>(tmp,v));
	++it;
      }
      else {
	modint_t tmp=(-extend(a)*jt->g)%m;
	r.coord.push_back(T_unsigned<modint_t,tdeg_t>(tmp,v));
      }
    }
    for (;it!=itend;++it){
      r.coord.push_back(*it);
    }
  }

  // p + q  -> r
  template<class tdeg_t,class modint_t>
  void smallmerge(polymod<tdeg_t,modint_t> & p,polymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & r){
    if (p.coord.empty()){
      swap(q.coord,r.coord);
      return;
    }
    if (q.coord.empty()){
      swap(p.coord,r.coord);
      return;
    }
    r.coord.clear();
    r.coord.reserve(p.coord.size()+q.coord.size());
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=q.coord.begin(),jtend=q.coord.end();
    for (;jt!=jtend;++jt){
      const tdeg_t & v=jt->u;
      for (;it!=itend && tdeg_t_strictly_greater(it->u,v,p.order);++it){
	r.coord.push_back(*it);
      }
      r.coord.push_back(*jt);
    }
    for (;it!=itend;++it){
      r.coord.push_back(*it);
    }
  }

  template<class tdeg_t,class modint_t>
  void smalladd(polymod<tdeg_t,modint_t> & p,polymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & r,modint_t env){
    if (p.coord.empty()){
      q.coord.swap(r.coord);
      return;
    }
    if (q.coord.empty()){
      p.coord.swap(r.coord);
      return;
    }
    r.coord.clear();
    r.coord.reserve(p.coord.size()+q.coord.size());
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=q.coord.begin(),jtend=q.coord.end();
    for (;jt!=jtend;++jt){
      const tdeg_t & v=jt->u;
      for (;it!=itend && tdeg_t_strictly_greater(it->u,v,p.order);++it){
	r.coord.push_back(*it);
      }
      if (it!=itend && it->u==jt->u){
        modint_t s=(it->g+extend(jt->g))%env;
        if (!is_zero(s))
          r.coord.push_back(T_unsigned<modint_t,tdeg_t>(s,it->u));
        ++it;
      }
      else
        r.coord.push_back(*jt);
    }
    for (;it!=itend;++it){
      r.coord.push_back(*it);
    }
  }

  template<class tdeg_t,class modint_t>
  void reducemod(const polymod<tdeg_t,modint_t> & p,const vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,unsigned excluded,polymod<tdeg_t,modint_t> & rem,modint_t env,bool topreduceonly=false){
    if (&p!=&rem)
      rem=p;
    if (p.coord.empty())
      return ;
    polymod<tdeg_t,modint_t> TMP2(p.order,p.dim);
    unsigned i,rempos=0;
    for (unsigned count=0;;++count){
      // this branch search first in all leading coeff of G for a monomial 
      // <= to the current rem monomial
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator pt=rem.coord.begin()+rempos;
      if (pt>=rem.coord.end())
	break;
      for (i=0;i<G.size();++i){
	if (i==excluded || res[G[i]].coord.empty())
	  continue;
	if (tdeg_t_all_greater(pt->u,res[G[i]].coord.front().u,p.order))
	  break;
      }
      if (i==G.size()){ // no leading coeff of G is smaller than the current coeff of rem
	++rempos;
	if (topreduceonly)
	  break;
	// if (small0) TMP1.coord.push_back(*pt);
	continue;
      }
      modint_t a(pt->g),b(res[G[i]].coord.front().g);
      if (pt->u==res[G[i]].coord.front().u){
	smallmultsubmod(rem,smod(extend(a)*invmod(b,env),env),res[G[i]],TMP2,env);
	// Gpos=i; // assumes basis element in G are sorted wrt >
      }
      else
	smallmultsubmodshift(rem,0,smod(extend(a)*invmod(b,env),env),res[G[i]],pt->u-res[G[i]].coord.front().u,TMP2,env);
      swap(rem.coord,TMP2.coord);
    }
    if (!rem.coord.empty()
#if 1 // ndef GBASIS_4PRIMES
        && rem.coord.front().g!=1
#endif
        ){
      smallmultmod(invmod(rem.coord.front().g,env),rem,env);
      rem.coord.front().g=create<modint_t>(1);
    }
  }
 
#if 0
  // reduce with respect to itself the elements of res with index in G
  template<class tdeg_t,class modint_t>
  void reducemod(vectpolymod<tdeg_t,modint_t> & res,vector<unsigned> G,modint env){
    if (res.empty() || G.empty())
      return;
    polymod<tdeg_t,modint_t> pred(res.front().order,res.front().dim),
      TMP2(res.front().order,res.front().dim);
    vectpolymod<tdeg_t,modint_t> q;
    // reduce res
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      polymod<tdeg_t,modint_t> & p=res[i];
      reducemod(p,res,G,i,q,pred,TMP2,env);
      swap(res[i].coord,pred.coord);
      pred.sugar=res[i].sugar;
    }
  }
#endif

  template<class tdeg_t,class modint_t>
  modint_t spolymod(const polymod<tdeg_t,modint_t> & p,const polymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & res,polymod<tdeg_t,modint_t> & TMP1,modint_t env){
    if (p.coord.empty()){
      res=q;
      return create<modint_t>(1);
    }
    if (q.coord.empty()){
      res= p;
      return create<modint_t>(1);
    }
    const tdeg_t & pi = p.coord.front().u;
    const tdeg_t & qi = q.coord.front().u;
    tdeg_t lcm;
    index_lcm(pi,qi,lcm,p.order);
    //polymod<tdeg_t,modint_t> TMP1(p);
    TMP1=p;
    // polymod<tdeg_t,modint_t> TMP2(q);
    const polymod<tdeg_t,modint_t> &TMP2=q;
    modint_t a=p.coord.front().g,b=q.coord.front().g;
    tdeg_t pshift=lcm-pi;
    unsigned sugarshift=pshift.total_degree(p.order);
    // adjust sugar/logz for res
    res.sugar=p.sugar+sugarshift;
    res.logz=p.logz+q.logz;
    // CERR << "spoly mod " << res.sugar << " " << pi << qi << '\n';
    if (p.order.o==_PLEX_ORDER || sugarshift!=0)
      smallshift(TMP1.coord,pshift,TMP1.coord);
    // smallmultmod(b,TMP1,env);
    if (lcm==qi)
      smallmultsubmod(TMP1,smod(extend(a)*invmod(b,env),env),TMP2,res,env);
    else
      smallmultsubmodshift(TMP1,0,smod(extend(a)*invmod(b,env),env),TMP2,lcm-qi,res,env);
    modint_t d=create<modint_t>(1);
    if (!res.coord.empty()
#if 1 // ndef GBASIS_4PRIMES
        && res.coord.front().g!=1
#endif
        ){
      d=invmod(res.coord.front().g,env);
      smallmultmod(d,res,env);
      res.coord.front().g=create<modint_t>(1);
    }
    if (debug_infolevel>2)
      CERR << "spolymod " << res << '\n';
    return d;
  }

  template<class tdeg_t,class modint_t>
  void reduce1smallmod(polymod<tdeg_t,modint_t> & p,const polymod<tdeg_t,modint_t> & q,polymod<tdeg_t,modint_t> & TMP2,modint_t env){
    if (p.coord.empty())
      return ;
    unsigned rempos=0;
    const tdeg_t & u = q.coord.front().u;
    const modint_t invg=invmod(q.coord.front().g,env);
    for (unsigned count=0;;++count){
      // this branch search first in all leading coeff of G for a monomial 
      // <= to the current rem monomial
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator pt=p.coord.begin()+rempos;
      if (pt>=p.coord.end())
	break;
      if (pt->u==u){
	smallmultsubmodshift(p,0,smod(extend(pt->g)*invg,env),q,pt->u-u,TMP2,env);
	swap(p.coord,TMP2.coord);
	break;
      }
      if (!tdeg_t_all_greater(pt->u,u,p.order)){
	++rempos;
	// TMP1.coord.push_back(*pt);
	continue;
      }
      smallmultsubmodshift(p,0,smod(extend(pt->g)*invg,env),q,pt->u-u,TMP2,env);
      // smallmultsubmodshift(p,rempos,smod(extend(pt->g)*invmod(g,env),env),q,pt->u-u,TMP2,env);
      rempos=0; 
      swap(p.coord,TMP2.coord);
    }
    // if (small0) swap(p.coord,TMP1.coord);
    if (!p.coord.empty() && p.coord.front().g!=1){
      smallmultmod(invmod(p.coord.front().g,env),p,env);
      p.coord.front().g=create<modint_t>(1);
    }
  }

#define GIAC_GBASIS_PERMUTATION1
#define GIAC_GBASIS_PERMUTATION2
  template<class tdeg_t>
  struct zsymb_data {
    unsigned pos;
    tdeg_t deg;
    order_t o;
    unsigned terms;
    int age;
    double coeffs;
  };

#ifdef GIAC_GBASIS_PERMUTATION2
  template<class tdeg_t>
  bool tri(const zsymb_data<tdeg_t> & z1,const zsymb_data<tdeg_t> & z2){
    int d1=z1.deg.total_degree(z1.o),d2=z2.deg.total_degree(z2.o);
    // beware that ordering must be stable across successives modular runs
    //if (z1.coeffs*d1!=z2.coeffs*d2) return z1.coeffs*d1<z2.coeffs*d2;
    //if (z1.coeffs+d1!=z2.coeffs+d2) return z1.coeffs+d1<z2.coeffs+d2;
    if (z1.coeffs!=z2.coeffs) return z1.coeffs<z2.coeffs;
    if (z1.pos!=z2.pos)
      return z1.pos<z2.pos;
    if (d1!=d2) return d1<d2;
    if (z1.terms!=z2.terms) 
      return z1.terms<z2.terms;
    return false;
  }
#endif

//#define GIAC_DEG_FIRST
  
  template <class tdeg_t>
  bool operator < (const zsymb_data<tdeg_t> & z1,const zsymb_data<tdeg_t> & z2){
    // reductor choice: less terms is better 
    // but small degree gives a reductor sooner
    // e.g. less terms is faster for botana* but slower for cyclic*
    // if (z1.terms!=z2.terms){ return z1.terms<z2.terms; }
    int d1=z1.deg.total_degree(z1.o),d2=z2.deg.total_degree(z2.o);
#ifdef GIAC_DEG_FIRST
    if (d1!=d2)
      return d1<d2;
#endif
    // double Z1=d1*double(z1.terms)*(z1.age+1); double Z2=d2*double(z2.terms)*(z2.age+1); if (Z1!=Z2) return Z2>Z1;
    //double Z1=z1.terms*double(z1.terms)/d1; double Z2=z2.terms*double(z2.terms)/d2; if (Z1!=Z2) return Z1<Z2;
    double Z1=z1.terms*double(z1.terms)*d1; double Z2=z2.terms*double(z2.terms)*d2; if (Z1!=Z2) return Z2>Z1;
    //double Z1=double(z1.terms)/d1; double Z2=double(z2.terms)/d2; if (Z1!=Z2) return Z2>Z1;
    //double Z1=double(z1.terms)*d1; double Z2=double(z2.terms)*d2; if (Z1!=Z2) return Z2>Z1;
    if (z1.terms!=z2.terms) return z2.terms>z1.terms;
    if (z1.deg!=z2.deg)
      return tdeg_t_greater(z1.deg,z2.deg,z1.o)!=0;
    if (z1.pos!=z2.pos)
      return z2.pos>z1.pos;
    return false;
  }

  // rewrite sum(A[i]*F[i]) mod env. By decreasing degree of A[i]*F[i]
  // if a monomial of A[i] is divisible by the leading monomial of F[j]
  // and A[j]*F[j] has a monomial of same degree
  // replace A[i] by A[i]-quotient*F[j] and A[j] by A[j]+quotient*F[i]
  // this will modify only monomials with smaller degree
  template<class tdeg_t,class modint_t>
  void reduceAF(vectpolymod<tdeg_t,modint_t> & A,const vectpolymod<tdeg_t,modint_t> & F,modint_t env,order_t order){
    return; // no visible effect
    polymod<tdeg_t,modint_t> tmp;
    int s=F.size();
    vector <tdeg_t> lF;
    for (unsigned i=0;i<s;++i)
      lF.push_back(F[i].coord.front().u);
    vector<int> startpos(s);
    while (1){
      tdeg_t aifideg;
      bool prev=false;
      vector<int> pos;
      // find degree
      for (int i=0;i<s;++i){
        if (startpos[i]<A[i].coord.size()){
          tdeg_t curdeg=A[i].coord[startpos[i]].u+lF[i];
          if (prev){
            if (curdeg==aifideg){
              pos.push_back(i);
              continue;
            }
            if (tdeg_t_greater(curdeg,aifideg,order)){
              pos.clear(); pos.push_back(i);
              aifideg=curdeg;
            }
          }
          else {
            pos.push_back(i);
            aifideg=curdeg;
            prev=true;
          }
        }
      }
      if (pos.empty()) // nothing left
        break;
      bool found=false;
      // check for divisibility
      for (int i=0;!found && i<pos.size()-1;++i){
        int posi=pos[i];
        aifideg=A[posi].coord[startpos[posi]].u;
        for (int j=i+1;j<pos.size();++j){
          int posj=pos[j];
          if (tdeg_t_all_greater(aifideg,lF[posj],order)){
            found=true;
            modint_t a=A[posi].coord[startpos[posi]].g;
            modint_t b=F[posj].coord[0].g;
            modint_t q=smod(extend(a)*invmod(b,env),env);
            tdeg_t du=aifideg-lF[posj];
            // replace A[posi] by A[posi]-quotient*F[posj]
            // and A[posj]] by A[posj]+quotient*F[posi]
            smallmultsubmodshift(A[posi],0,q,F[posj],du,tmp,env);
            A[posi].coord.swap(tmp.coord);
            smallmultsubmodshift(A[posj],0,-q,F[posi],du,tmp,env);
            A[posj].coord.swap(tmp.coord);
            break;
          }
        }
      }
      if (!found){
        // increase positions
        for (int i=0;i<pos.size();++i){
          ++startpos[pos[i]];
        }
      }
    }
  }

  template <class tdeg_t,class modint_t>
  double sumdegcoeffs(const vectpolymod<tdeg_t,modint_t> & V,const order_t & o){
    double res=0;
    for (size_t j=0;j<V.size();++j){
      res += V[j].coord.empty()?0:V[j].coord.front().u.total_degree(o);
    }
    return res;
  }

  template <class tdeg_t,class modint_t>
  double sumtermscoeffs(const vectpolymod<tdeg_t,modint_t> & V){
    double res=0;
    for (size_t j=0;j<V.size();++j){
      res += V[j].coord.size();
    }
    return res;
  }

/* 
strategy=6999?
Singular uses different heuristics for the sorting, dependently on the ideal (homogeneous or not) and the ordering (block/lex vs degree).

When issuing test(55), it is shown which decisions for the selected Gröbner basis computation will be made.

This is it:

  lift   7> list l=lift(i,1);
  red: redLiftstd
  posInT: posInT_EcartpLength
  posInL: posInL15
  enterS: enterSBba
  initEcart: initEcartBBA
  initEcartPair: initEcartPairMora
  homog=0, LazyDegree=1, LazyPass=2, ak=15,
  honey=1, sugarCrit=0, Gebauer=0, noTailReduction=0, use_buckets=1
  chainCrit: chainCritNormal
  posInLDependsOnLength=0
  //options: redTail redThrough intStrategy redefine usage prompt 53 55
  LDeg: pLDegb / pLDegb
  currRing->pFDeg: ? (7f04a96862a0)
   syzring:1, syzComp(strat):1 limit:1

Here:
 T: is the set of reductors, sorted by length(number of monomials), then Ecart
 L: the set of pairs, sorted by deg(leading term)+Ecart, then by monomial order
 S: the Gröbner basis (subset of T)
 redLiftstd: the reduction of S-Polynomial s:
   chosen first element p from T such that divides

Bis hierhin ist "Polynom" das eigentliche Polynom und (als hintere
Terme) die Herleitung (das geht insbesonderer in die Laenge mit ein).
Fuer die Reduktion wird es jedoch wieder geteilt und mit den
"Herleitungsteil" nur eine "lazy computation" durchgefuehrt.
Nur bei Erfolg (d.h. der Anfang redziert nicht zu 0)
wird dieese ausgefuehrt (siehe redLiftstd)

Until this point "Polynom" is the actual polynomial and (as terms behind)
the coefficients (this is very important for the length).
For reduction it will be, however, divided again, and with the "coefficients part"
there will be only a "lazy computation" done.
This will be performed only in case of success (i.e. the leading will be reduced not to 0)
(see redLiftstd).
 */
  template<class tdeg_t,class modint_t>
  void reducesmallmod(polymod<tdeg_t,modint_t> & rem,const vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,unsigned excluded,modint_t env,polymod<tdeg_t,modint_t> & TMP1,bool normalize,int start_index=0,bool topreduceonly=false,vectpolymod<tdeg_t,modint_t>*remcoeffsptr=0,vector< vectpolymod<tdeg_t,modint_t> > * coeffsmodptr=0,int strategy=0){
    vector< polymod<tdeg_t,modint_t> > addtoremcoeffs(remcoeffsptr?remcoeffsptr->size():0);
    bool usemap=strategy/10000000;
#ifdef HASHMAP_NAMESPACE
    vector< map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t>> > mapremcoeffs;
    if (remcoeffsptr && usemap){
      HASHMAP_NAMESPACE::hash_map<tdeg_t,modint_t> m(obj);
      mapremcoeffs=vector< HASHMAP_NAMESPACE::hash_map<tdeg_t,modint_t> >(remcoeffsptr->size(),m);
      for (size_t k=0;k<remcoeffsptr->size();++k){
        poly2map((*remcoeffsptr)[k],mapremcoeffs[k]);
      }      
    }
#else
    vector< map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> > > mapremcoeffs;
    if (remcoeffsptr && usemap){
      tdeg_t_sort_t<tdeg_t> obj(rem.order);
      map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> > m(obj);
      mapremcoeffs=vector< map<tdeg_t,modint_t,tdeg_t_sort_t<tdeg_t> > >(remcoeffsptr->size(),m);
      for (size_t k=0;k<remcoeffsptr->size();++k){
        poly2map((*remcoeffsptr)[k],mapremcoeffs[k]);
      }      
    }
#endif
    if (strategy>=0){
      strategy /= 1000;
      strategy %= 1000;
    }
    if (debug_infolevel>1000){
      rem.dbgprint();
      if (!rem.coord.empty()) rem.coord.front().u.dbgprint();
    }
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator pt,ptend;
    unsigned i,rempos=0;
    TMP1.coord.clear();
    unsigned Gs=unsigned(G.size());
    const order_t o=rem.order;
    int Gstart_index=0;
    // starting at excluded may fail because the batch of new basis element created
    // by f4mod is reduced with respect to the previous batches but not necessarily
    // with respect to itself
    if (start_index && excluded<Gs){
      for (int i=excluded;i>=0;--i){
	int Gi=G[i];
	if (Gi<=start_index){
	  Gstart_index=i;
	  break;
	}
      }
    }
#ifdef GIAC_GBASIS_PERMUTATION2
    if (excluded<Gs) 
      excluded=G[excluded];
    else
      excluded=-1;
    vector< zsymb_data<tdeg_t> > zsGi(Gs);
    for (unsigned i=0;i<Gs;++i){
      int Gi=G[i];
      const polymod<tdeg_t,modint_t> & cur=res[Gi];
      zsymb_data<tdeg_t> tmp={(unsigned)Gi,cur.coord.empty()?0:cur.coord.front().u,o,(unsigned)cur.coord.size(),0,0.0};
      if (coeffsmodptr && strategy>=0){
        double D=sumdegcoeffs((*coeffsmodptr)[Gi],o),T=sumtermscoeffs((*coeffsmodptr)[Gi]),N=(*coeffsmodptr)[Gi].size(),d=cur.coord.front().u.total_degree(o),t=cur.coord.size();
        if (strategy==1 || strategy==0)
          tmp.coeffs = D;
        else if (strategy==11)
          tmp.coeffs = T*t*double(d);
        else if (strategy==2)
          tmp.coeffs = D*T;
        else if (strategy==3)
          tmp.coeffs = (N*d+D)*(N*t+T);
        else if (strategy==4)
          tmp.coeffs = N*d+D;
        else if (strategy==5)
          tmp.coeffs = N*t+T;
        else if (strategy==6)
          tmp.coeffs = T;
        else if (strategy==7)
          tmp.coeffs = D*t;
        else if (strategy==8)
          tmp.coeffs = D*(N*t+T);
        else if (strategy==9)
          tmp.coeffs = D*t*T;
        else if (strategy==10)
          tmp.coeffs = D*d*t*T;
      }
      zsGi[i]=tmp;
    }
    sort(zsGi.begin(),zsGi.end(),tri<tdeg_t>); //reverse(zsGi.begin(),zsGi.end());
#else
    const tdeg_t ** resGi=(const tdeg_t **) malloc(Gs*sizeof(tdeg_t *));
    for (unsigned i=0;i<Gs;++i){
      resGi[i]=res[G[i]].coord.empty()?0:&res[G[i]].coord.front().u;
    }
#endif
    for (unsigned count=0;;++count){
      ptend=rem.coord.end();
      // this branch search first in all leading coeff of G for a monomial 
      // <= to the current rem monomial
      pt=rem.coord.begin()+rempos;
      if (pt>=ptend)
	break;
      const tdeg_t &ptu=pt->u;
#ifdef GIAC_GBASIS_PERMUTATION2
      for (i=0;i<Gs;++i){
	zsymb_data<tdeg_t> & zs=zsGi[i];
	if (zs.terms && zs.pos!=excluded && tdeg_t_all_greater(ptu,zs.deg,o))
	  break;
      }
#else // GIAC_GBASIS_PERMUTATION2
      if (excluded<Gs){
	i=Gs;
	const tdeg_t ** resGi_=resGi+excluded+1,**resGiend=resGi+Gs;
	for (;resGi_<resGiend;++resGi_){
	  if (!*resGi_)
	    continue;
	  if (tdeg_t_all_greater(ptu,**resGi_,o)){
	    i=unsigned(resGi_-resGi);
	    break;
	  }
	}
	if (i==Gs){
	  resGi_=resGi+Gstart_index;
	  resGiend=resGi+excluded;
	  for (;resGi_<resGiend;++resGi_){
	    if (!*resGi_)
	      continue;
	    if (tdeg_t_all_greater(ptu,**resGi_,o)){
	      i=unsigned(resGi_-resGi);
	      break;
	    }
	  }
	}
      }
      else {
	const tdeg_t ** resGi_=resGi,**resGiend=resGi+Gs;
	for (;resGi_<resGiend;++resGi_){
	  if (!*resGi_)
	    continue;
#if 0
	  int res=tdeg_t_compare_all(ptu,**resGi_,o);
	  if (res==1) break;
	  if (res==-1) 
	    *resGi_=0;
#else	    
	  if (tdeg_t_all_greater(ptu,**resGi_,o)) 
	    break;
#endif
	}
	i=unsigned(resGi_-resGi);
      }
#endif // GIAC_GBASIS_PERMUTATION2
      if (i==G.size()){ // no leading coeff of G is smaller than the current coeff of rem
	++rempos;
	if (topreduceonly)
	  break;
	// if (small0) TMP1.coord.push_back(*pt);
	continue;
      }
#ifdef GIAC_GBASIS_PERMUTATION2
      int Gi=zsGi[i].pos;
#else
      int Gi=G[i];
#endif
      modint_t a(pt->g),b(res[Gi].coord.front().g),c(smod(a*extend(invmod(b,env)),env));
      tdeg_t du(pt->u-res[Gi].coord.front().u);
      smallmultsubmodshift(rem,0,c,res[Gi],du,TMP1,env);
      // smallmultsub(rem,rempos,smod(a*invmod(b,env->modulo),env->modulo).val,res[G[i]],pt->u-res[G[i]].coord.front().u,TMP2,env->modulo.val);
      // rempos=0; // since we have removed the beginning of rem (copied in TMP1)
      swap(rem.coord,TMP1.coord);
      if (debug_infolevel>3)
        CERR << "du=" << du << "\n";
      if (remcoeffsptr){
	// reflect linear combination on remcoeffs
        if (usemap){
          for (size_t k=0;k<mapremcoeffs.size();++k){
            if ((*coeffsmodptr)[Gi][k].coord.size())
              mapmultsubmodshift<tdeg_t,modint_t>(mapremcoeffs[k],c,(*coeffsmodptr)[Gi][k],du,env);
          }        
        }
        else {
          vectpolymod<tdeg_t,modint_t> & remcoeffs=*remcoeffsptr;
          for (size_t k=0;k<addtoremcoeffs.size();++k){
            addtoremcoeffs[k].order=o;
            smallmultsubmodshift(addtoremcoeffs[k],0,c,(*coeffsmodptr)[Gi][k],du,TMP1,env);
            swap(addtoremcoeffs[k].coord,TMP1.coord);
            if (addtoremcoeffs[k].coord.size()>remcoeffs[k].coord.size()){
              smalladd(remcoeffs[k],addtoremcoeffs[k],TMP1,env);
              swap(remcoeffs[k].coord,TMP1.coord);
              addtoremcoeffs[k].coord.clear();
            }
          }
        } // end else usemap
      }
      continue;
    }
    if (remcoeffsptr){
      // reflect linear combination on remcoeffs
      vectpolymod<tdeg_t,modint_t> & remcoeffs=*remcoeffsptr;
      if (usemap){
        for (size_t k=0;k<remcoeffs.size();++k){
          map2poly(mapremcoeffs[k],remcoeffs[k]);
        }
      }
      else {
        for (size_t k=0;k<remcoeffs.size();++k){
          if (!addtoremcoeffs[k].coord.empty()){
            smalladd(remcoeffs[k],addtoremcoeffs[k],TMP1,env);
            swap(remcoeffs[k].coord,TMP1.coord);
          }
        }
      }
    }
    if (normalize && !rem.coord.empty()
#if 1 // ndef GBASIS_4PRIMES
        && rem.coord.front().g!=1
#endif
        ){
      // smallmult does %, smallmultmod also make the result positive
      // smallmult(invmod(rem.coord.front().g,env),rem.coord,rem.coord,env);
      modint_t a(invmod(rem.coord.front().g,env));
      smallmultmod(a,rem,env,false);
      rem.coord.front().g=create<modint_t>(1);
      if (remcoeffsptr){
	// reflect on remcoeffs
	vectpolymod<tdeg_t,modint_t> & remcoeffs=*remcoeffsptr;
	for (size_t k=0;k<remcoeffs.size();++k){
	  smallmultmod(a,remcoeffs[k],env,false);
	}
      }
    }
#ifndef GIAC_GBASIS_PERMUTATION2
    free(resGi);
#endif
  }

  template<class tdeg_t,class modint_t>
  static void reducemod(vectpolymod<tdeg_t,modint_t> &resmod,modint env){
    if (resmod.empty())
      return;
    // Initial interreduce step
    polymod<tdeg_t,modint_t> TMP1(resmod.front().order,resmod.front().dim);
    vector<unsigned> G(resmod.size());
    for (unsigned j=0;j<G.size();++j)
      G[j]=j;
    for (unsigned j=0; j<resmod.size();++j){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      reducesmallmod(resmod[j],resmod,G,j,env,TMP1,true);
    }
  }

  template<class tdeg_t,class modint_t>
  void gbasis_updatemod(vector<unsigned> & G,vector< paire > & B,vectpolymod<tdeg_t,modint_t> & res,unsigned pos,polymod<tdeg_t,modint_t> & TMP2,modint_t env,bool reduce,const vector<unsigned> & oldG){
    if (debug_infolevel>2)
      CERR << CLOCK()*1e-6 << " mod begin gbasis update " << G.size() << '\n';
    if (debug_infolevel>3)
      CERR << G << '\n';
    const polymod<tdeg_t,modint_t> & h = res[pos];
    if (h.coord.empty())
      return;
    order_t order=h.order;
    vector<unsigned> C;
    C.reserve(G.size()+1);
    const tdeg_t & h0=h.coord.front().u;
    // FIXME: should use oldG instead of G here
    for (unsigned i=0;i<oldG.size();++i){
      if (tdeg_t_all_greater(h0,res[oldG[i]].coord.front().u,order))
	return;
    }
    tdeg_t tmp1,tmp2;
    // C is used to construct new pairs
    // create pairs with h and elements g of G, then remove
    // -> if g leading monomial is prime with h, remove the pair
    // -> if g leading monomial is not disjoint from h leading monomial
    //    keep it only if lcm of leading monomial is not divisible by another one
#if 1
    size_t tmpsize=G.size();
    vector<tdeg_t> tmp(tmpsize);
    for (unsigned i=0;i<tmpsize;++i){
      if (res[G[i]].coord.empty()){
	tmp[i].tab[0]=-2;
      }
      else
	index_lcm(h0,res[G[i]].coord.front().u,tmp[i],order); 
    }
#else
    // this would be faster but it does not work for 
    // gbasis([25*y^2*x^6-10*y^2*x^5+59*y^2*x^4-20*y^2*x^3+43*y^2*x^2-10*y^2*x+9*y^2-80*y*x^6+136*y*x^5+56*y*x^4-240*y*x^3+104*y*x^2+64*x^6-192*x^5+192*x^4-64*x^3,25*y^2*6*x^5-10*y^2*5*x^4+59*y^2*4*x^3-20*y^2*3*x^2+43*y^2*2*x-10*y^2-80*y*6*x^5+136*y*5*x^4+56*y*4*x^3-240*y*3*x^2+104*y*2*x+64*6*x^5-192*5*x^4+192*4*x^3-64*3*x^2,25*2*y*x^6-10*2*y*x^5+59*2*y*x^4-20*2*y*x^3+43*2*y*x^2-10*2*y*x+9*2*y-80*x^6+136*x^5+56*x^4-240*x^3+104*x^2],[x,y],revlex);
    // pair <4,3> is not generated
    unsigned tmpsize=G.empty()?0:G.back()+1;
    vector<tdeg_t> tmp(tmpsize);
    for (unsigned i=0;i<tmpsize;++i){
      if (res[i].coord.empty()){
	tmp[i].tab[0]=-2;
      }
      else
	index_lcm(h0,res[i].coord.front().u,tmp[i],order); 
    }
#endif
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (res[G[i]].coord.empty() || disjoint(h0,res[G[i]].coord.front().u,res.front().order,res.front().dim))
	continue;
      // h0 and G[i] leading monomial not prime together
#if 1
      tdeg_t * tmp1=&tmp[i]; 
#else
      tdeg_t * tmp1=&tmp[G[i]];
#endif
      tdeg_t * tmp2=&tmp[0],*tmpend=tmp2+tmpsize;
      for (;tmp2!=tmp1;++tmp2){
	if (tmp2->tab[0]<0)
	  continue;
	if (tdeg_t_all_greater(*tmp1,*tmp2,order))
	  break; // found another pair, keep the smallest, or the first if equal
      }
      if (tmp2!=tmp1)
	continue;
      for (++tmp2;tmp2<tmpend;++tmp2){
	if (tmp2->tab[0]<0)
	  continue;
	if (tdeg_t_all_greater(*tmp1,*tmp2,order) && *tmp1!=*tmp2){
	  break; 
	}
      }
      if (tmp2==tmpend)
	C.push_back(G[i]);
    }
    vector< paire > B1;
    B1.reserve(B.size()+C.size());
    for (unsigned i=0;i<B.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (res[B[i].first].coord.empty() || res[B[i].second].coord.empty())
	continue;
      index_lcm(res[B[i].first].coord.front().u,res[B[i].second].coord.front().u,tmp1,order);
      if (!tdeg_t_all_greater(tmp1,h0,order)){
	B1.push_back(B[i]);
	continue;
      }
      index_lcm(res[B[i].first].coord.front().u,h0,tmp2,order);
      if (tmp2==tmp1){
	B1.push_back(B[i]);
	continue;
      }
      index_lcm(res[B[i].second].coord.front().u,h0,tmp2,order);
      if (tmp2==tmp1){
	B1.push_back(B[i]);
	continue;
      }
    }
    // B <- B union pairs(h,g) with g in C
    for (unsigned i=0;i<C.size();++i){
      B1.push_back(paire(pos,C[i]));
    }
    swap(B1,B);
    // Update G by removing elements with leading monomial >= leading monomial of h
    if (debug_infolevel>2){
      CERR << CLOCK()*1e-6 << " end, pairs:"<< '\n';
      if (debug_infolevel>3)
	CERR << B << '\n';
      CERR << "mod begin Groebner interreduce " << '\n';
    }
    C.clear();
    C.reserve(G.size()+1);
    // bool pos_pushed=false;
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (!res[G[i]].coord.empty() && !tdeg_t_all_greater(res[G[i]].coord.front().u,h0,order)){
	if (reduce){
	  // reduce res[G[i]] with respect to h
	  reduce1smallmod(res[G[i]],h,TMP2,env);
	}
	C.push_back(G[i]);
      }
      // NB: removing all pairs containing i in it does not work
    }
    if (debug_infolevel>2)
      CERR << CLOCK()*1e-6 << " mod end Groebner interreduce " << '\n';
    C.push_back(pos);
    swap(C,G);
#if 0
    // clear in res polymod<tdeg_t,modint_t> that are no more referenced
    vector<bool> used(res.size(),false);
    for (unsigned i=0;i<G.size();++i){
      used[G[i]]=true;
    }
    for (unsigned i=0;i<B.size();++i){
      used[B[i].first]=true;
      used[B[i].first]=false;
    }
    for (unsigned i=0;i<res.size();++i){
      if (!used[i] && !res[i].coord.capacity()){
	polymod<tdeg_t,modint_t> clearer;
	swap(res[i].coord,clearer.coord);
      }
    }
#endif
  }

#if 0
  // update G, G is a list of index of the previous gbasis + new spolys
  // new spolys index are starting at debut
  template<class tdeg_t,class modint_t>
  void gbasis_multiupdatemod(vector<unsigned> & G,vector< paire > & B,vectpolymod<tdeg_t,modint_t> & res,unsigned debut,polymod<tdeg_t,modint_t> & TMP2,modint_t env){
    if (debug_infolevel>2)
      CERR << CLOCK()*1e-6 << " mod begin gbasis update " << G.size() << "+" << add.size() << '\n';
    if (debug_infolevel>3)
      CERR << G << '\n';
    vector<unsigned> C;
    // C is used to construct new pairs
    tdeg_t tmp1,tmp2;
    order_t order;
    for (unsigned pos=debut;pos<G.size();++pos){
      const polymod<tdeg_t,modint_t> & h = res[pos];
      const tdeg_t & h0=h.coord.front().u;
      // create pairs with h and elements g of G, then remove
      // -> if g leading monomial is prime with h, remove the pair
      // -> if g leading monomial is not disjoint from h leading monomial
      //    keep it only if lcm of leading monomial is not divisible by another one
      for (unsigned i=0;i<pos;++i){
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return;
	if (res[G[i]].coord.empty() || disjoint(h0,res[G[i]].coord.front().u,res.front().order,res.front().dim))
	  continue;
	index_lcm(h0,res[G[i]].coord.front().u,tmp1,order); // h0 and G[i] leading monomial not prime together
	unsigned j;
	for (j=0;j<G.size();++j){
	  if (i==j || res[G[j]].coord.empty())
	    continue;
	  index_lcm(h0,res[G[j]].coord.front().u,tmp2,order);
	  if (tdeg_t_all_greater(tmp1,tmp2,order)){
	    // found another pair, keep the smallest, or the first if equal
	    if (tmp1!=tmp2)
	      break; 
	    if (i>j)
	      break;
	  }
	} // end for j
	if (j==G.size())
	  C.push_back(G[i]);
      }
    }
    vector< paire > B1;
    B1.reserve(B.size()+C.size());
    for (unsigned i=0;i<B.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (res[B[i].first].coord.empty() || res[B[i].second].coord.empty())
	continue;
      index_lcm(res[B[i].first].coord.front().u,res[B[i].second].coord.front().u,tmp1,order);
      for (unsigned pos=debut;pos<G.size();++pos){
	const tdeg_t & h0=res[G[pos]].front().u;
	if (!tdeg_t_all_greater(tmp1,h0,order)){
	  B1.push_back(B[i]);
	  break;
	}
	index_lcm(res[B[i].first].coord.front().u,h0,tmp2,order);
	if (tmp2==tmp1){
	  B1.push_back(B[i]);
	  break;
	}
	index_lcm(res[B[i].second].coord.front().u,h0,tmp2,order);
	if (tmp2==tmp1){
	  B1.push_back(B[i]);
	  break;
	}
      }
    }
    // B <- B union B2
    for (unsigned i=0;i<B2.size();++i)
      B1.push_back(B2[i]);
    swap(B1,B);
    // Update G by removing elements with leading monomial >= leading monomial of h
    if (debug_infolevel>2){
      CERR << CLOCK()*1e-6 << " end, pairs:"<< '\n';
      if (debug_infolevel>3)
	CERR << B << '\n';
      CERR << "mod begin Groebner interreduce " << '\n';
    }
    vector<unsigned> C;
    C.reserve(G.size());
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (res[G[i]].coord.empty())
	continue;
      const tdeg_t & Gi=res[G[i]].coord.front().u;
      unsigned j;
      for (j=i+1;j<G.size();++j){
	if (tdeg_t_all_greater(Gi,res[G[j]].coord.front().u,order))
	  break;
      }
      if (i==G.size()-1 || j==G.size())
	C.push_back(G[i]);
    }
    if (debug_infolevel>2)
      CERR << CLOCK()*1e-6 << " mod end Groebner interreduce " << '\n';
    swap(C,G);
  }
#endif

  template<class tdeg_t,class modint_t>
  bool in_gbasismod(vectpoly8<tdeg_t> & res8,vectpolymod<tdeg_t,modint_t> &res,vector<unsigned> & G,modint_t env,bool sugar,vector< paire > * pairs_reducing_to_zero){
    convert(res8,res,env);
    unsigned ressize=unsigned(res8.size());
    unsigned learned_position=0;
    bool learning=pairs_reducing_to_zero && pairs_reducing_to_zero->empty();
    if (debug_infolevel>1000)
      res.dbgprint(); // instantiate dbgprint()
    polymod<tdeg_t,modint_t> TMP1(res.front().order,res.front().dim),TMP2(res.front().order,res.front().dim);
    vector< paire > B;
    order_t order=res.front().order;
    if (order.o==_PLEX_ORDER)
      sugar=false;
    vector<unsigned> oldG(G);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " initial reduction/gbasis_updatemod: " << ressize << '\n';
    for (unsigned l=0;l<ressize;++l){
#ifdef GIAC_REDUCEMODULO
      reducesmallmod(res[l],res,G,-1,env,TMP2,env!=0);
#endif      
      gbasis_updatemod(G,B,res,l,TMP2,env,true,oldG);
    }
    for (;!B.empty() && !interrupted && !ctrl_c;){
      oldG=G;
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " mod number of pairs: " << B.size() << ", base size: " << G.size() << '\n';
      // find smallest lcm pair in B
      tdeg_t small0,cur;
      unsigned smallpos,smallsugar=0,cursugar=0;
      for (smallpos=0;smallpos<B.size();++smallpos){
	if (!res[B[smallpos].first].coord.empty() && !res[B[smallpos].second].coord.empty())
	  break;
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return false;
      }
      index_lcm(res[B[smallpos].first].coord.front().u,res[B[smallpos].second].coord.front().u,small0,order);
      if (sugar)
	smallsugar=res[B[smallpos].first].sugar+(small0-res[B[smallpos].first].coord.front().u).total_degree(order);
      for (unsigned i=smallpos+1;i<B.size();++i){
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return false;
	if (res[B[i].first].coord.empty() || res[B[i].second].coord.empty())
	  continue;
	bool doswap=false;
	index_lcm(res[B[i].first].coord.front().u,res[B[i].second].coord.front().u,cur,order);
	if (sugar)
	  cursugar=res[B[smallpos].first].sugar+(cur-res[B[smallpos].first].coord.front().u).total_degree(order);
	if (order.o==_PLEX_ORDER)
	  doswap=tdeg_t_strictly_greater(small0,cur,order);
	else {
	  if (cursugar!=smallsugar)
	    doswap = smallsugar > cursugar;
	  else
	    doswap=tdeg_t_strictly_greater(small0,cur,order);
	}
	if (doswap){
	  // CERR << "swap mod " << cursugar << " " << res[B[i].first].coord.front().u << " " << res[B[i].second].coord.front().u << '\n';
	  swap(small0,cur); // small0=cur;
	  swap(smallsugar,cursugar);
	  smallpos=i;
	}
      }
      paire bk=B[smallpos];
      B.erase(B.begin()+smallpos);
      if (pairs_reducing_to_zero && learned_position<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[learned_position]){
	++learned_position;
	continue;
      }
      if (debug_infolevel>1 && (equalposcomp(G,bk.first)==0 || equalposcomp(G,bk.second)==0))
	CERR << CLOCK()*1e-6 << " mod reducing pair with 1 element not in basis " << bk << '\n';
      // polymod<tdeg_t,modint_t> h(res.front().order,res.front().dim);
      spolymod<tdeg_t,modint_t>(res[bk.first],res[bk.second],TMP1,TMP2,env);
      if (debug_infolevel>1){
	CERR << CLOCK()*1e-6 << " mod reduce begin, pair " << bk << " spoly size " << TMP1.coord.size() << " sugar deg " << TMP1.sugar << " degree " << TMP1.coord.front().u << '\n';
      }
      reducemod(TMP1,res,G,-1,TMP1,env);
      if (debug_infolevel>1){
	if (debug_infolevel>2){ CERR << TMP1 << '\n'; }
	CERR << CLOCK()*1e-6 << " mod reduce end, remainder size " << TMP1.coord.size() << '\n';
      }
      if (!TMP1.coord.empty()){
	if (ressize==res.size())
	  res.push_back(polymod<tdeg_t,modint_t>(TMP1.order,TMP1.dim));
	swap(res[ressize],TMP1);
	++ressize;
	gbasis_updatemod(G,B,res,ressize-1,TMP2,env,true,oldG);
	if (debug_infolevel>2)
	  CERR << CLOCK()*1e-6 << " mod basis indexes " << G << " pairs indexes " << B << '\n';
      }
      else {
	if (learning && pairs_reducing_to_zero)
	  pairs_reducing_to_zero->push_back(bk);
      }
    }
    if (ressize<res.size())
      res.resize(ressize);
    // sort(res.begin(),res.end(),tripolymod<tdeg_t,modint_t>);
    convert(res,G,res8,env);
    return true;
  }

  // F4BUCHBERGER algorithm
  template<class tdeg_t>
  struct heap_tt {
    bool left;
    unsigned f4buchbergervpos:31;
    unsigned polymodpos;
    tdeg_t u;
    heap_tt(bool l,unsigned a,unsigned b,tdeg_t t):left(l),f4buchbergervpos(a),polymodpos(b),u(t){};
    heap_tt(unsigned a,unsigned b,tdeg_t t):left(true),f4buchbergervpos(a),polymodpos(b),u(t){};
    heap_tt():left(true),f4buchbergervpos(0),polymodpos(0),u(){};
  };

  template<class tdeg_t>
  struct heap_tt_compare {
    order_t order;
    const heap_tt<tdeg_t> * ptr;
    inline bool operator () (unsigned a,unsigned b){
      return !tdeg_t_greater((ptr+a)->u,(ptr+b)->u,order);
      // return (ptr+a)->u<(ptr+b)->u;
    }
    heap_tt_compare(const vector<heap_tt<tdeg_t> > & v,order_t o):order(o),ptr(v.empty()?0:&v.front()){};
  };


  template<class tdeg_t>
  struct compare_heap_tt {
    order_t order;
    inline bool operator () (const heap_tt<tdeg_t> & a,const heap_tt<tdeg_t> & b){
      return !tdeg_t_greater(a.u,b.u,order);
      // return (ptr+a)->u<(ptr+b)->u;
    }
    compare_heap_tt(order_t o):order(o) {}
  };


  // inline bool operator > (const heap_tt & a,const heap_tt & b){ return a.u>b.u; }

  // inline bool operator < (const heap_tt & a,const heap_tt & b){ return b>a;}

  template<class tdeg_t>
  struct heap_tt_ptr {
    heap_tt<tdeg_t> * ptr;
    heap_tt_ptr(heap_tt<tdeg_t> * ptr_):ptr(ptr_){};
    heap_tt_ptr():ptr(0){};
  };


  // inline bool operator > (const heap_tt_ptr & a,const heap_tt_ptr & b){ return a.ptr->u > b.ptr->u; }

  // inline bool operator < (const heap_tt_ptr & a,const heap_tt_ptr & b){ return b>a; }
  template<class tdeg_t>
  struct compare_heap_tt_ptr {
    order_t order;
    inline bool operator () (const heap_tt_ptr<tdeg_t> & a,const heap_tt_ptr<tdeg_t> & b){
      return !tdeg_t_greater(a.ptr->u,b.ptr->u,order);
      // return (ptr+a)->u<(ptr+b)->u;
    }
    compare_heap_tt_ptr(order_t o):order(o) {}
  };


  template<class tdeg_t,class modint_t>
  void collect(const vectpolymod<tdeg_t,modint_t> & f4buchbergerv,polymod<tdeg_t,modint_t> & allf4buchberger,int start=0){
    typename vectpolymod<tdeg_t,modint_t>::const_iterator it=f4buchbergerv.begin(),itend=f4buchbergerv.end();
    vector<heap_tt<tdeg_t> > Ht;
    vector<heap_tt_ptr<tdeg_t> > H; 
    Ht.reserve(itend-it);
    H.reserve(itend-it);
    unsigned s=0;
    order_t keyorder={_REVLEX_ORDER,0};
    for (unsigned i=0;it!=itend;++i,++it){
      keyorder=it->order;
      if (int(it->coord.size())>start){
	s=giacmax(s,unsigned(it->coord.size()));
	Ht.push_back(heap_tt<tdeg_t>(i,start,it->coord[start].u));
	H.push_back(heap_tt_ptr<tdeg_t>(&Ht.back()));
      }
    }
    allf4buchberger.coord.reserve(s); // int(s*std::log(1+H.size())));
    compare_heap_tt_ptr<tdeg_t> key(keyorder);
    make_heap(H.begin(),H.end(),key);
    while (!H.empty()){
      std::pop_heap(H.begin(),H.end(),key);
      // push root node of the heap in allf4buchberger
      heap_tt<tdeg_t> & current = *H.back().ptr;
      if (allf4buchberger.coord.empty() || allf4buchberger.coord.back().u!=current.u)
	allf4buchberger.coord.push_back(T_unsigned<modint_t,tdeg_t>(create<modint_t>(1),current.u));
      ++current.polymodpos;
      if (current.polymodpos>=f4buchbergerv[current.f4buchbergervpos].coord.size()){
	H.pop_back();
	continue;
      }
      current.u=f4buchbergerv[current.f4buchbergervpos].coord[current.polymodpos].u;
      std::push_heap(H.begin(),H.end(),key);
    }
  }

  template<class tdeg_t,class modint_t>
  void collect(const vectpolymod<tdeg_t,modint_t> & f4buchbergerv,const vector<unsigned> & G,polymod<tdeg_t,modint_t> & allf4buchberger,unsigned start=0){
    unsigned Gsize=unsigned(G.size());
    if (!Gsize) return;
    vector<heap_tt<tdeg_t> > H;
    compare_heap_tt<tdeg_t> key(f4buchbergerv[G[0]].order);
    H.reserve(Gsize);
    for (unsigned i=0;i<Gsize;++i){
      if (f4buchbergerv[G[i]].coord.size()>start)
	H.push_back(heap_tt<tdeg_t>(i,start,f4buchbergerv[G[i]].coord[start].u));
    }
    make_heap(H.begin(),H.end(),key);
    while (!H.empty()){
      std::pop_heap(H.begin(),H.end(),key);
      // push root node of the heap in allf4buchberger
      heap_tt<tdeg_t> & current =H.back();
      if (allf4buchberger.coord.empty() || allf4buchberger.coord.back().u!=current.u)
	allf4buchberger.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,current.u));
      ++current.polymodpos;
      if (current.polymodpos>=f4buchbergerv[G[current.f4buchbergervpos]].coord.size()){
	H.pop_back();
	continue;
      }
      current.u=f4buchbergerv[G[current.f4buchbergervpos]].coord[current.polymodpos].u;
      std::push_heap(H.begin(),H.end(),key);
    }
  }

  template<class tdeg_t,class modint_t>
  void leftright(const vectpolymod<tdeg_t,modint_t> & res,vector< paire > & B,vector<tdeg_t> & leftshift,vector<tdeg_t> & rightshift){
    for (unsigned i=0;i<B.size();++i){
      const polymod<tdeg_t,modint_t> & p=res[B[i].first];
      const polymod<tdeg_t,modint_t> & q=res[B[i].second];
      if (debug_infolevel>2)
	CERR << "leftright " << p << "," << q << '\n';
      tdeg_t l(p.coord.front().u);
      index_lcm(p.coord.front().u,q.coord.front().u,l,p.order);
      leftshift[i]=l-p.coord.front().u;
      rightshift[i]=l-q.coord.front().u;
    }
  }

  // collect monomials from pairs of res (vector of polymods), shifted by lcm
  // does not collect leading monomial (since they cancel)
  template<class tdeg_t,class modint_t>
  void collect(const vectpolymod<tdeg_t,modint_t> & res,vector< paire > & B,polymod<tdeg_t,modint_t> & allf4buchberger,vector<tdeg_t> & leftshift,vector<tdeg_t> & rightshift){
    int start=1;
    vector<heap_tt<tdeg_t> > Ht;
    vector<heap_tt_ptr<tdeg_t> > H; 
    Ht.reserve(2*B.size());
    H.reserve(2*B.size());
    unsigned s=0;
    order_t keyorder={_REVLEX_ORDER,0};
    for (unsigned i=0;i<B.size();++i){
      const polymod<tdeg_t,modint_t> & p=res[B[i].first];
      const polymod<tdeg_t,modint_t> & q=res[B[i].second];
      keyorder=p.order;
      if (int(p.coord.size())>start){
	s=giacmax(s,unsigned(p.coord.size()));
	Ht.push_back(heap_tt<tdeg_t>(true,i,start,p.coord[start].u+leftshift[i]));
	H.push_back(heap_tt_ptr<tdeg_t>(&Ht.back()));
      }
      if (int(q.coord.size())>start){
	s=giacmax(s,unsigned(q.coord.size()));
	Ht.push_back(heap_tt<tdeg_t>(false,i,start,q.coord[start].u+rightshift[i]));
	H.push_back(heap_tt_ptr<tdeg_t>(&Ht.back()));
      }
    }
    allf4buchberger.coord.reserve(s); // int(s*std::log(1+H.size())));
    compare_heap_tt_ptr<tdeg_t> key(keyorder);
    make_heap(H.begin(),H.end(),key);
    while (!H.empty()){
      std::pop_heap(H.begin(),H.end(),key);
      // push root node of the heap in allf4buchberger
      heap_tt<tdeg_t> & current = *H.back().ptr;
      if (allf4buchberger.coord.empty() || allf4buchberger.coord.back().u!=current.u)
	allf4buchberger.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,current.u));
      ++current.polymodpos;
      unsigned vpos;
      if (current.left)
	vpos=B[current.f4buchbergervpos].first;
      else
	vpos=B[current.f4buchbergervpos].second;
      if (current.polymodpos>=res[vpos].coord.size()){
	H.pop_back();
	continue;
      }
      if (current.left)
	current.u=res[vpos].coord[current.polymodpos].u+leftshift[current.f4buchbergervpos];
      else 
	current.u=res[vpos].coord[current.polymodpos].u+rightshift[current.f4buchbergervpos];
      std::push_heap(H.begin(),H.end(),key);
    }
  }

  struct sparse_element {
    modint val;
    unsigned pos;
    sparse_element(modint v,size_t u):val(v),pos(unsigned(u)){};
    sparse_element():val(0),pos(-1){};
  };

#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const sparse_element & s){
    return os << '{' << s.val<<',' << s.pos << '}' ;
  }
#else
  ostream & operator << (ostream & os,const sparse_element & s){
    return os << '{' << s.val<<',' << s.pos << '}' ;
  }
#endif

#ifdef GBASISF4_BUCHBERGER
  bool reducef4buchbergerpos(vector<modint> &v,const vector< vector<modint> > & M,vector<int> pivotpos,modint env){
    unsigned pos=0;
    bool res=false;
    for (unsigned i=0;i<M.size();++i){
      const vector<modint> & m=M[i];
      pos=pivotpos[i];
      if (pos==-1)
	return res;
      modint c=v[pos];
      if (!c)
	continue;
      res=true;
      c=(extend(invmod(m[pos],env))*c)%env;
      vector<modint>::const_iterator jt=m.begin()+pos+1;
      vector<modint>::iterator it=v.begin()+pos,itend=v.end();
      *it=0; ++it;
      for (;it!=itend;++jt,++it){
	if (*jt)
	  *it=(*it-extend(c)*(*jt))%env;
      }
    }
    return res;
  }

#ifdef x86_64
  unsigned reducef4buchberger_64(vector<modint> &v,const vector< vector<sparse_element> > & M,modint env,vector<int128_t> & w){
    w.resize(v.size());
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    vector<int128_t>::iterator wt=w.begin();
    for (;vt!=vtend;++wt,++vt){
      *wt=*vt;
    }
    for (unsigned i=0;i<M.size();++i){
      const vector<sparse_element> & m=M[i];
      const sparse_element * it=&m.front(),*itend=it+m.size(),*it2;
      if (it==itend)
	continue;
      int128_t & ww=w[it->pos];
      if (ww==0)
	continue;
      modint c=(extend(invmod(it->val,env))*ww)%env;
      // CERR << "multiplier ok line " << i << " value " << c << " " << w << '\n';
      if (!c)
	continue;
      ww=0;
      ++it;
      it2=itend-8;
      for (;it<=it2;){
#if 0
	w[it[0].pos] -= extend(c)*(it[0].val);
	w[it[1].pos] -= extend(c)*(it[1].val);
	w[it[2].pos] -= extend(c)*(it[2].val);
	w[it[3].pos] -= extend(c)*(it[3].val);
	w[it[4].pos] -= extend(c)*(it[4].val);
	w[it[5].pos] -= extend(c)*(it[5].val);
	w[it[6].pos] -= extend(c)*(it[6].val);
	w[it[7].pos] -= extend(c)*(it[7].val);
	it+=8;
#else
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
#endif
      }
      for (;it!=itend;++it){
	w[it->pos] -= extend(c)*(it->val);
      }
    }
    for (vt=v.begin(),wt=w.begin();vt!=vtend;++wt,++vt){
      if (*wt)
	*vt=*wt % env;
      else
	*vt=0;
    }
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return vt-v.begin();
    }
    return v.size();
  }

#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const int128_t & i){
    return os << extend(i) ;
    // return os << "(" << extend(i>>64) <<","<< extend(i) <<")" ;
  }
#else
  ostream & operator << (ostream & os,const int128_t & i){
    return os << extend(i) ;
    // return os << "(" << extend(i>>64) <<","<< extend(i) <<")" ;
  }
#endif

#endif
  // sparse element if prime is < 2^24
  // if shift == 0 the position is absolute in the next sparse32 of the vector
  struct sparse32 {
    modint val:25;
    unsigned shift:7;
    sparse32(modint v,unsigned s):val(v),shift(s){};
    sparse32():val(0),shift(0){};
  };

#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const sparse32 & s){
    return os << "(" << s.val << "," << s.shift << ")" ;
  }
#else
  ostream & operator << (ostream & os,const sparse32 & s){
    return os << "(" << s.val << "," << s.shift << ")" ;
  }
#endif

  unsigned reducef4buchberger_32(vector<modint> &v,const vector< vector<sparse32> > & M,modint env,vector<modint2> & w){
    w.resize(v.size());
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    vector<modint2>::iterator wt=w.begin();
    for (;vt!=vtend;++wt,++vt){
      *wt=*vt;
    }
    for (unsigned i=0;i<M.size();++i){
      const vector<sparse32> & m=M[i];
      vector<sparse32>::const_iterator it=m.begin(),itend=m.end(),it2=itend-16;
      if (it==itend)
	continue;
      unsigned p=0;
      modint val;
      if (it->shift){
	p += it->shift;
	val=it->val;
      }
      else {
	val=it->val;
	++it;
	p=*(unsigned *)&(*it);
      }
      modint2 & ww=w[p];
      if (ww==0)
	continue;
      modint c=(extend(invmod(val,env))*ww)%env;
      if (!c)
	continue;
      ww=0;
      ++it;
      for (;it<=it2;){
	sparse32 se = *it;
	unsigned seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
	se = *it;
	seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
	se = *it;
	seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
	se = *it;
	seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
	se = *it;
	seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
	se = *it;
	seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
	se = *it;
	seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
	se = *it;
	seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
	++it;
      }
      for (;it!=itend;++it){
	const sparse32 & se = *it;
	unsigned seshift=se.shift;
	if (seshift){
	  p += seshift;
	  w[p] -= extend(c)*se.val;
	}
	else {
	  ++it;
	  p = *(unsigned *) &*it;
	  w[p] -= extend(c)*se.val;
	}
      }
    }
    for (vt=v.begin(),wt=w.begin();vt!=vtend;++wt,++vt){
      if (*wt)
	*vt = *wt % env;
      else
	*vt =0;
    }
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return unsigned(vt-v.begin());
    }
    return unsigned(v.size());
  }

#ifdef PSEUDO_MOD
  // find pseudo remainder of x mod p, 2^nbits>=p>2^(nbits-1)
  // assumes invp=2^(2*nbits)/p+1 has been precomputed 
  // and abs(x)<2^(31+nbits)
  // |remainder| <= max(2^nbits,|x|*p/2^(2nbits)), <=2*p if |x|<=p^2
  inline int pseudo_mod(longlong x,int p,unsigned invp,unsigned nbits){
    return int(x - (((x>>nbits)*invp)>>(nbits))*p);
  }
  // a <- (a+b*c) mod or smod p
  inline void pseudo_mod(int & a,int b,int c,int p,unsigned invp,unsigned nbits){
    a=pseudo_mod(a+((longlong)b)*c,p,invp,nbits);
  }
#endif

  unsigned reducef4buchberger(vector<modint> &v,const vector< vector<sparse_element> > & M,modint env){
#ifdef PSEUDO_MOD
    int nbits=sizeinbase2(env);
    unsigned invmodulo=((1ULL<<(2*nbits)))/env+1;
#endif
    for (unsigned i=0;i<M.size();++i){
      const vector<sparse_element> & m=M[i];
      vector<sparse_element>::const_iterator it=m.begin(),itend=m.end();
      if (it==itend)
	continue;
      modint c=(extend(invmod(it->val,env))*v[it->pos])%env;
      v[it->pos]=0;
      if (!c)
	continue;
#ifdef PSEUDO_MOD
      if (env<(1<<29)){
	c=-c;
	for (++it;it!=itend;++it){
	  pseudo_mod(v[it->pos],c,it->val,env,invmodulo,nbits);
	}
	continue;
      }
#endif
      for (++it;it!=itend;++it){
	modint &x=v[it->pos];
	x=(x-extend(c)*(it->val))%env;
      }
    }
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
#ifdef PSEUDO_MOD
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	*vt %= env;
    }
#endif
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return unsigned(vt-v.begin());
    }
    return unsigned(v.size());
  }


#if GIAC_SHORTSHIFTTYPE==8
  typedef unsigned char shifttype; 
  // assumes that all shifts are less than 2^(3*sizeof()), 
  // and almost all shifts are less than 2^sizeof()-1
  // for unsigned char here matrix density should be significantly above 0.004

  inline void next_index(unsigned & pos,const shifttype * & it){
    if (*it)
      pos+=(*it);
    else { // next 3 will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += (*it << 8);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<modint>::iterator & pos,const shifttype * & it){
    if (*it)
      pos+=(*it);
    else { // next 3 will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += (*it << 8);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<modint2>::iterator & pos,const shifttype * & it){
    if (*it)
      pos+=(*it);
    else { // next 3 will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += (*it << 8);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<double>::iterator & pos,const shifttype * & it){
    if (*it)
      pos+=(*it);
    else { // next 3 will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += (*it << 8);
      ++it;
      pos += *it;
    }
    ++it;
  }

#ifdef x86_64
  inline void next_index(vector<int128_t>::iterator & pos,const shifttype * & it){
    if (*it)
      pos+=(*it);
    else { // next 3 will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += (*it << 8);
      ++it;
      pos += *it;
    }
    ++it;
  }
#endif


  unsigned first_index(const vector<shifttype> & v){
    if (v.front())
      return v.front();
    return (v[1]<<16)+(v[2]<<8)+v[3];
  }

  inline void pushsplit(vector<shifttype> & v,unsigned & pos,unsigned newpos){
    unsigned shift=newpos-pos;
    if (shift && (shift <(1<<8)))
      v.push_back(shift);
    else {
      v.push_back(0);
      v.push_back(shift >> 16 );
      v.push_back(shift >> 8);
      v.push_back(shift);
    }
    pos=newpos;
  }
#endif

#if GIAC_SHORTSHIFTTYPE==16
  typedef unsigned short shifttype; 

  inline void next_index(unsigned & pos,const shifttype * & it){
    if (*it)
      pos += (*it);
    else { // next will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<modint>::iterator & pos,const shifttype * & it){
    if (*it)
      pos += (*it);
    else { // next will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<mod4int>::iterator & pos,const shifttype * & it){
    if (*it)
      pos += (*it);
    else { // next will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<mod4int2>::iterator & pos,const shifttype * & it){
    if (*it)
      pos += (*it);
    else { // next will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<modint2>::iterator & pos,const shifttype * & it){
    if (*it)
      pos += (*it);
    else { // next will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += *it;
    }
    ++it;
  }

  inline void next_index(vector<double>::iterator & pos,const shifttype * & it){
    if (*it)
      pos += (*it);
    else { // next will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += *it;
    }
    ++it;
  }

#ifdef x86_64
  inline void next_index(vector<int128_t>::iterator & pos,const shifttype * & it){
    if (*it)
      pos += (*it);
    else { // next will make the shift
      ++it;
      pos += (*it << 16);
      ++it;
      pos += *it;
    }
    ++it;
  }
#endif

  unsigned first_index(const vector<shifttype> & v){
    if (v.front())
      return v.front();
    return (v[1]<<16)+v[2];
  }

  inline void pushsplit(vector<shifttype> & v,unsigned & pos,unsigned newpos){
    unsigned shift=newpos-pos;
    if ( shift && (shift < (1<<16)) )
      v.push_back(shift);
    else {
      v.push_back(0);
      v.push_back(shift >> 16 );
      v.push_back(shift);
    }
    pos=newpos;
  }
#endif

#ifndef GIAC_SHORTSHIFTTYPE
  typedef unsigned shifttype; 
  inline void next_index(unsigned & pos,const shifttype * & it){
    pos=(*it);
    ++it;
  }
  inline unsigned first_index(const vector<shifttype> & v){
    return v.front();
  }
  inline void pushsplit(vector<shifttype> & v,unsigned & pos,unsigned newpos){
    v.push_back(pos=newpos);
  }

#endif

  struct coeffindex_t {
    bool b;
    unsigned u:24;
    coeffindex_t(bool b_,unsigned u_):b(b_),u(u_) {};
    coeffindex_t():b(false),u(0) {};
  };

#ifdef x86_64
  unsigned reducef4buchbergersplit128(vector<modint> &v,const vector< vector<shifttype> > & M,const vector<unsigned> & firstpos,vector< vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env,vector<int128_t> & v128){
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    v128.resize(v.size());
    vector<int128_t>::iterator wt=v128.begin(),wt0=wt;
    for (;vt!=vtend;++wt,++vt)
      *wt=*vt;
    vector<unsigned>::const_iterator fit=firstpos.begin(),fit0=fit,fitend=firstpos.end();
    for (;fit!=fitend;++fit){
      if (*(wt0+*fit)==0)
	continue;
      unsigned i=fit-fit0;
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      bool shortshifts=coeffindex[i].b;
      vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end(),jt_=jtend-8;
      if (jt==jtend)
	continue;
      const vector<shifttype> & mindex=M[i];
      const shifttype * it=&mindex.front();
      unsigned pos=0;
      next_index(pos,it);
      wt=wt0+pos;
      // if (*wt==0) continue;
      // if (pos>v.size()) CERR << "error" <<'\n';
      modint c=(invmod(*jt,env)*(*wt))%env;
      *wt=0;
      if (!c)
	continue;
      ++jt;
#ifdef GIAC_SHORTSHIFTTYPE
      if (shortshifts){
#if 0
	if (jt<jt_){
	  while (uextend(it)%4){
	    wt += *it; ++it;;
	    *wt -=extend(c)*(*jt);
	    ++jt;
	  }
	}
#endif
	for (;jt<jt_;){
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;	
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  wt += *it; ++it;;
	  *wt -=extend(c)*(*jt);
	  ++jt;	
	}
      } // if (shortshifts)
      else {
	for (;jt<jt_;){
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;	
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;
	  next_index(wt,it);
	  *wt -=extend(c)*(*jt);
	  ++jt;	
	}
      }
#else // GIAC_SHORTSHIFTTYPE
      for (;jt<jt_;){
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
      }
#endif
      for (;jt!=jtend;++jt){
#ifdef GIAC_SHORTSHIFTTYPE
	next_index(wt,it);
	*wt -=extend(c)*(*jt);
#else
	v128[*it]-=extend(c)*(*jt);
	++it;
#endif
      }
    }
    unsigned res=v.size();
    for (vt=v.begin(),wt=v128.begin();vt!=vtend;++wt,++vt){
      int128_t i=*wt;
      if (i)
	i %= env;
      *vt = i;
      if (i){
	res=vt-v.begin();
	break;
      }
    }
    for (;vt!=vtend;++wt,++vt){
      if (*wt)
	*vt = *wt % env;
      else
	*vt = 0;
    }
    return res;
  }

  unsigned reducef4buchbergersplit128u(vector<modint> &v,const vector< vector<unsigned> > & M,vector< vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env,vector<int128_t> & v128){
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    v128.resize(v.size());
    vector<int128_t>::iterator wt=v128.begin();
    for (;vt!=vtend;++wt,++vt)
      *wt=*vt;
    for (unsigned i=0;i<M.size();++i){
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end(),jt_=jtend-8;
      if (jt==jtend)
	continue;
      const vector<unsigned> & mindex=M[i];
      const unsigned * it=&mindex.front();
      unsigned pos=*it;
      // if (pos>v.size()) CERR << "error" <<'\n';
      modint c=(invmod(*jt,env)*v128[pos])%env;
      v128[pos]=0;
      if (!c)
	continue;
      ++it;++jt;
      for (;jt<jt_;){
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
      }
      for (;jt!=jtend;++jt){
	v128[*it]-=extend(c)*(*jt);
	++it;
      }
    }
    for (vt=v.begin(),wt=v128.begin();vt!=vtend;++wt,++vt){
      if (*wt)
	*vt = *wt % env;
      else
	*vt=0;
    }
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return vt-v.begin();
    }
    return v.size();
  }

  unsigned reducef4buchbergersplit128s(vector<modint> &v,const vector< vector<short unsigned> > & M,vector< vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env,vector<int128_t> & v128){
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    v128.resize(v.size());
    vector<int128_t>::iterator wt=v128.begin();
    for (;vt!=vtend;++wt,++vt)
      *wt=*vt;
    for (unsigned i=0;i<M.size();++i){
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end(),jt_=jtend-8;
      if (jt==jtend)
	continue;
      const vector<short unsigned> & mindex=M[i];
      const short unsigned * it=&mindex.front();
      unsigned pos=*it;
      // if (pos>v.size()) CERR << "error" <<'\n';
      modint c=(invmod(*jt,env)*v128[pos])%env;
      v128[pos]=0;
      if (!c)
	continue;
      ++it;++jt;
      for (;jt<jt_;){
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
	v128[*it]-=extend(c)*(*jt);
	++it; ++jt;
      }
      for (;jt!=jtend;++jt){
	v128[*it]-=extend(c)*(*jt);
	++it;
      }
    }
    for (vt=v.begin(),wt=v128.begin();vt!=vtend;++wt,++vt){
      if (*wt)
	*vt = *wt % env;
      else
	*vt=0;
    }
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return vt-v.begin();
    }
    return v.size();
  }

#endif

  // This version is not the fastest one, see below with 64 bits for v
  // reducef4buchberger matrix M has band structure, to spare memory
  // we split coeffs/index in :
  // - M each line is a list of shift index, 
  // - coeffindex, relative to coeffs, M[i][j] corresponds to coeffs[coeffinde[i]][j]
  // - coeffs is the list of coefficients
  unsigned reducef4buchbergersplit(vector<modint> &v,const vector< vector<shifttype> > & M,const vector<unsigned> & firstpos,vector< vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env,vector<modint2> & v64){
    vector<modint>::iterator vt=v.begin(),vt0=vt,vtend=v.end();
    vector<unsigned>::const_iterator fit=firstpos.begin(),fit0=fit,fitend=firstpos.end();
    if (env<(1<<24)){
      v64.resize(v.size());
      vector<modint2>::iterator wt=v64.begin(),wt0=wt,wtend=v64.end();
      for (;vt!=vtend;++wt,++vt){
	*wt=*vt;
	*vt=0;
      }
      bool fastcheck = (fitend-fit0)<=0xffff;
      for (;fit!=fitend;++fit){
	if (fastcheck && *(wt0+*fit)==0)
	  continue;
	unsigned i=unsigned(fit-fit0);
	if (!fastcheck){ 
	  if ((i&0xffff)==0xffff){
	    // reduce the line mod env
	    for (wt=v64.begin();wt!=wtend;++wt){
	      if (*wt)
		*wt %= env;
	    }
	  }
	  if (v64[*fit]==0)
	    continue;
	}
	const vector<shifttype> & mindex=M[i];
	const shifttype * it=&mindex.front();
	unsigned pos=0;
	next_index(pos,it);
	wt=wt0+pos;
	// if (*wt==0) continue;
	const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
	bool shortshifts=coeffindex[i].b;
	if (mcoeff.empty())
	  continue;
	const modint * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
	// if (pos>v.size()) CERR << "error" <<'\n';
	// if (*jt!=1) CERR << "not normalized" << '\n';
	modint c=(extend(invmod(*jt,env))*(*wt % env))%env;
	*wt=0;
	if (!c)
	  continue;
	++jt;
#ifdef GIAC_SHORTSHIFTTYPE
	if (shortshifts){
	  for (;jt<jt_;){
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	    ++jt;
	  }
	  for (;jt!=jtend;++jt){
	    wt += *it; ++it;
	    *wt-=extend(c)*(*jt);
	  }
	}
	else {
	  for (;jt!=jtend;++jt){
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	  }
	}
#else // def GIAC_SHORTSHIFTTYPE
	for (;jt<jt_;){
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	}
	for (;jt!=jtend;++jt){
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	}
#endif // def GIAC_SHORTSHIFTTYPE
      }
      unsigned res=unsigned(v.size());
      for (vt=v.begin(),wt=v64.begin();vt!=vtend;++wt,++vt){
	modint2 i=*wt;
	if (i) // if (i>=env || i<=-env)
	  i %= env;
	*vt = shrink(i);
	if (i){
	  res=unsigned(vt-v.begin());
	  break;
	}
      }
      for (;vt!=vtend;++wt,++vt){
	if (modint2 i=*wt) // if (i>=env || i<=-env)
	  *vt = i % env;
      }
      return res;
    }
#ifdef PSEUDO_MOD
    int nbits=sizeinbase2(env);
    unsigned invmodulo=((1ULL<<(2*nbits)))/env+1;
#endif
    for (;fit!=fitend;++fit){
      if (*(vt0+*fit)==0)
	continue;
      unsigned i=unsigned(fit-fit0);
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      bool shortshifts=coeffindex[i].b;
      vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end(),jt_=jt-8;
      if (jt==jtend)
	continue;
      const vector<shifttype> & mindex=M[i];
      const shifttype * it=&mindex.front();
      unsigned pos=0;
      next_index(pos,it);
      vt=v.begin()+pos;
      // if (pos>v.size()) CERR << "error" <<'\n';
      modint c=(extend(invmod(*jt,env))*(*vt))%env;
      *vt=0;
      if (!c)
	continue;
      ++jt;
#ifdef PSEUDO_MOD
      if (env<(1<<29)){
	c=-c;
#ifdef GIAC_SHORTSHIFTTYPE
	if (shortshifts){
	  for (;jt<jt_;){
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	    ++jt;
	  }
	  for (;jt!=jtend;++jt){
	    vt += *it; ++it;
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	  }
	}
	else {
	  for (;jt<jt_;){
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	    next_index(vt,it); pseudo_mod(*vt,c,*jt,env,invmodulo,nbits); ++jt;
	  }
	  for (;jt!=jtend;++jt){
	    next_index(vt,it);
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(*vt,c,*jt,env,invmodulo,nbits);
	  }
	}
	continue;
#else
	for (;jt!=jtend;++jt){
	  // if (pos>v.size()) CERR << "error" <<'\n';
	  pseudo_mod(v[*it],c,*jt,env,invmodulo,nbits);
	  ++it;
	}
	continue;
#endif // GIAC_SHORTSHIFTTYPES
      } // end if env<1<<29
#endif // PSEUDOMOD
      for (;jt!=jtend;++jt){
#ifdef GIAC_SHORTSHIFTTYPE
	next_index(vt,it);
	*vt = (*vt-extend(c)*(*jt))%env;
#else
	modint &x=v[*it];
	++it;
	x=(x-extend(c)*(*jt))%env;
#endif
      }
    }
    vt=v.begin();vtend=v.end();
#ifdef PSEUDO_MOD
    unsigned res=v.size();
    for (;vt!=vtend;++vt){ // or if (*vt) *vt %= env;
      if (!*vt) continue;
      *vt %= env;
      if (*vt){
	res=vt-v.begin();
	break;
      }
    }
    for (;vt!=vtend;++vt){ // or if (*vt) *vt %= env;
      modint v=*vt;
      if (v>-env && v<env)
	continue;
      *vt = v % env;
    }
    return res;
#endif
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return unsigned(vt-v.begin());
    }
    return unsigned(v.size());
  }

  inline void special_mod(modint2 & x,modint2 c,modint d,modint env,modint2 env2){
#if 0
    x -= c*d;
    x += ((x>>63) & env2);
#else
    register modint2 y=x-c*d; // 1 read, 1 write, 5 instr
    x = y + ((y>>63)&env2);
#endif
    //x=y%env;
    //if (y<0) x = y+env2; else x=y;// if y is negative make it positive by adding env^2
    // x = y - (y>>63)*env2;
  }

  inline void special_mod(modint2 & x,const modint & c,const modint & d,const modint2 & env2){
    register modint2 y=x-extend(c)*d;
    x = y + ((y>>63)&env2);
  }

  inline void special_mod(double & x,double c,modint d,modint env,double env2){
    register modint2 y=x-c*d;
    if (y<0) x = double(y+env2); else x=double(y);// if y is negative make it positive by adding env^2
  }

  typedef char used_t;
  // typedef bool used_t;

  void f4_innerloop_(modint2 * wt,const modint * jt,const modint * jtend,modint c,const shifttype* it){
    jtend -= 8;
    for (;jt<=jtend;){
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
      ++jt;
    }
    jtend+=8;
    for (;jt!=jtend;++jt){
      wt += *it; ++it;
      *wt-=extend(c)*(*jt);
    }
  }

  inline void f4_innerloop(modint2 * wt,const modint * jt,const modint * jtend,modint C,const shifttype* it){
    jtend -= 16;
    for (;jt<=jtend;){
#if 1
      wt += it[0]; int b=it[1];
      *wt -= extend(C)*jt[0];
      wt[b] -= extend(C)*jt[1];
      wt += b+it[2]; b=it[3];
      *wt -= extend(C)*jt[2];
      wt[b] -= extend(C)*jt[3];
      wt += b+it[4]; b=it[5];
      *wt -= extend(C)*jt[4];
      wt[b] -= extend(C)*jt[5];
      wt += b+it[6]; b=it[7];
      *wt -= extend(C)*jt[6];
      wt[b] -= extend(C)*jt[7];
      wt += b+it[8]; b=it[9];
      *wt -= extend(C)*jt[8];
      wt[b] -= extend(C)*jt[9];
      wt += b+it[10]; b=it[11];
      *wt -= extend(C)*jt[10];
      wt[b] -= extend(C)*jt[11];
      wt += b+it[12]; b=it[13];
      *wt -= extend(C)*jt[12];
      wt[b] -= extend(C)*jt[13];
      wt += b+it[14]; b=it[15];
      *wt -= extend(C)*jt[14];
      wt[b] -= extend(C)*jt[15];
      wt += b;
      it += 16; jt+=16;
#else
      wt += it[0]; *wt -= extend(C)*jt[0];
      wt += it[1]; *wt -= extend(C)*jt[1];
      wt += it[2]; *wt -= extend(C)*jt[2];
      wt += it[3]; *wt -= extend(C)*jt[3];
      wt += it[4]; *wt -= extend(C)*jt[4];
      wt += it[5]; *wt -= extend(C)*jt[5];
      wt += it[6]; *wt -= extend(C)*jt[6];
      wt += it[7]; *wt -= extend(C)*jt[7];
      wt += it[8]; *wt -= extend(C)*jt[8];
      wt += it[9]; *wt -= extend(C)*jt[9];
      wt += it[10]; *wt -= extend(C)*jt[10];
      wt += it[11]; *wt -= extend(C)*jt[11];
      wt += it[12]; *wt -= extend(C)*jt[12];
      wt += it[13]; *wt -= extend(C)*jt[13];
      wt += it[14]; *wt -= extend(C)*jt[14];
      wt += it[15]; *wt -= extend(C)*jt[15];
      it += 16; jt+=16;
#endif
    }
    jtend += 16;
    for (;jt!=jtend;++jt){
      wt += *it; ++it;
      *wt-=extend(C)*(*jt);
    }
  }

  void f4_innerloop_special_mod(modint2 * wt,const modint * jt,const modint * jtend,modint C,const shifttype* it,modint env){
    modint2 env2=extend(env)*env;
    if (jtend-jt>3 && ((ulonglong) it &0x2)){ // align it address
      // should be always true (since we have already read one time)
      wt += *it; ++it;
      special_mod(*wt,C,*jt,env2); ++jt;
      if (0 && (ulonglong) it &0x4){
	wt += *it; ++it;
	special_mod(*wt,C,*jt,env2); ++jt;
	wt += *it; ++it;
	special_mod(*wt,C,*jt,env2); ++jt;
      }
    }
    jtend -= 16;
#ifndef BIDGENDIAN  // it address is 32 bits aligned
#if 0 // def CPU_SIMD// vectorization is not faster
    unsigned * IT=(unsigned *)it;
    Vec4q CC(C),P(env2);
    for (;jt<=jtend;){ // 6 pointers (4wt, 1jt, 1IT), 1 unsigned, 1 modint, 1 modint2
      unsigned B; modint2 * wt0,*wt1,*wt2;
      B=*IT;
      wt += (B&0xffff);
      wt0=wt;
      wt += (B>>16);
      wt1=wt;
      B=IT[1];
      wt += (B&0xffff);
      wt2=wt;
      wt += (B>>16); // 12 instr, 2 read
      Vec4q A(*wt0,*wt1,*wt2,*wt); // 1 load, 4 reads
      Vec4i D; D.load(jt); // 1 load from 1 pointer
      A -= CC*extend(D); // simd 1* 1-
      A += ((A>>63)&P); // simd 1>> 1& 1+
      *wt0=A.extract(0); // 6 instr, 4 write
      *wt1=A.extract(1);
      *wt2=A.extract(2);
      *wt=A.extract(3);
      IT+=2; jt+=4; // 18 instr + 6 reads + 4 write + 5 simd + 1 simd read
    }
    it=(shifttype *) IT;
#else // simd instructions
    unsigned * IT=(unsigned *) it;
    for (;jt<=jtend;){
      unsigned B;
      B=*IT; //1+1read
      wt += (B&0xffff);  // 2
      special_mod(*wt,C,*jt,env2); // 5+2read/1write
      wt += (B>>16); // 2
      special_mod(*wt,C,jt[1],env2); //5+2R+1W
      B=IT[1]; // 1+1read
      wt += (B&0xffff); // 2
      special_mod(*wt,C,jt[2],env2); // 5+2R+1W
      wt += (B>>16); // 2
      special_mod(*wt,C,jt[3],env2);  //5+2R+1W => 30 instr + 10 reads + 4 write
      B=IT[2];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[4],env2);
      wt += (B>>16);;
      special_mod(*wt,C,jt[5],env2); 
      B=IT[3];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[6],env2);
      wt += (B>>16);;
      special_mod(*wt,C,jt[7],env2); 
      B=IT[4];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[8],env2);
      wt += (B>>16);;
      special_mod(*wt,C,jt[9],env2); 
      B=IT[5];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[10],env2);
      wt += (B>>16);;
      special_mod(*wt,C,jt[11],env2); 
      B=IT[6];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[12],env2);
      wt += (B>>16);;
      special_mod(*wt,C,jt[13],env2); 
      B=IT[7];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[14],env2);
      wt += (B>>16);;
      special_mod(*wt,C,jt[15],env2); 
      IT += 8; jt+=16;
    }
    it=(shifttype *) IT;
#endif // simd instructions
#else
    for (;jt<=jtend;){
      wt += it[0]; int b=it[1];
      special_mod(*wt,C,*jt,env2); 
      special_mod(wt[b],C,jt[1],env2); 
      wt += b+it[2]; b=it[3];
      special_mod(*wt,C,jt[2],env2); 
      special_mod(wt[b],C,jt[3],env2); 
      wt += b+it[4]; b=it[5];
      special_mod(*wt,C,jt[4],env2); 
      special_mod(wt[b],C,jt[5],env2); 
      wt += b+it[6]; b=it[7];
      special_mod(*wt,C,jt[6],env2); 
      special_mod(wt[b],C,jt[7],env2); 
      wt += b+it[8]; b=it[9];
      special_mod(*wt,C,jt[8],env2); 
      special_mod(wt[b],C,jt[9],env2); 
      wt += b+it[10]; b=it[11];
      special_mod(*wt,C,jt[10],env2); 
      special_mod(wt[b],C,jt[11],env2); 
      wt += b+it[12]; b=it[13];
      special_mod(*wt,C,jt[12],env2); 
      special_mod(wt[b],C,jt[13],env2); 
      wt += b+it[14]; b=it[15];
      special_mod(*wt,C,jt[14],env2); 
      special_mod(wt[b],C,jt[15],env2); 
      wt += b;
      it += 16; jt+=16;
    }
#endif
    jtend += 16;
    for (;jt!=jtend;++jt){
      wt += *it; ++it;
      special_mod(*wt,C,*jt,env2); 
    }
  }

  void f4_innerloop_special_mod(double * wt,const modint * jt,const modint * jtend,modint C,const shifttype* it,modint env){
    double env2=double(env)*env;
    jtend -= 16;
    for (;jt<=jtend;){
      wt += it[0]; int b=it[1];
      special_mod(*wt,C,*jt,env,env2); 
      special_mod(wt[b],C,jt[1],env,env2); 
      wt += b+it[2]; b=it[3];
      special_mod(*wt,C,jt[2],env,env2); 
      special_mod(wt[b],C,jt[3],env,env2); 
      wt += b+it[4]; b=it[5];
      special_mod(*wt,C,jt[4],env,env2); 
      special_mod(wt[b],C,jt[5],env,env2); 
      wt += b+it[6]; b=it[7];
      special_mod(*wt,C,jt[6],env,env2); 
      special_mod(wt[b],C,jt[7],env,env2); 
      wt += b+it[8]; b=it[9];
      special_mod(*wt,C,jt[8],env,env2); 
      special_mod(wt[b],C,jt[9],env,env2); 
      wt += b+it[10]; b=it[11];
      special_mod(*wt,C,jt[10],env,env2); 
      special_mod(wt[b],C,jt[11],env,env2); 
      wt += b+it[12]; b=it[13];
      special_mod(*wt,C,jt[12],env,env2); 
      special_mod(wt[b],C,jt[13],env,env2); 
      wt += b+it[14]; b=it[15];
      special_mod(*wt,C,jt[14],env,env2); 
      special_mod(wt[b],C,jt[15],env,env2); 
      wt += b;
      it += 16; jt+=16;
    }
    jtend += 16;
    for (;jt!=jtend;++jt){
      wt += *it; ++it;
      special_mod(*wt,C,*jt,env,env2); 
    }
  }

  template <class modt,class modint_t>
  unsigned store_coeffs(vector<modt> &v64or32,unsigned firstcol,vector<modint_t> & lescoeffs,unsigned * bitmap,vector<used_t> & used,modint_t env){
    unsigned res=0;
    used_t * uit=&used.front();
    typename vector<modt>::iterator wt0=v64or32.begin(),wt=v64or32.begin()+firstcol,wtend=v64or32.end();
    typename vector<modt>::iterator wt1=wtend-4;
#if 1
    for (;wt<=wt1;wt+=4){
      if (!is_zero(wt[0]) | !is_zero(wt[1]) | !is_zero(wt[2]) | !is_zero(wt[3]) )
	break;
    }
#endif
    if (!res){
      for (;wt<wtend;++wt){
	modt i=*wt;
	if (is_zero(i))
          continue;
	*wt = create<modt>(0);
	i %= env;
	if (is_zero(i))
          continue;
	unsigned I=unsigned(wt-wt0);
	res=I;
	*(uit+I)=1; // used[i]=1;
	bitmap[I>>5] |= (1<<(I&0x1f));
	lescoeffs.push_back(shrink(i));
	break;
      }
      if (!res)
	res=unsigned(v64or32.size());
    }
#if 1
    for (;wt<=wt1;){
      modt i=*wt;
      if (is_zero(i)){
	if (is_zero(wt[1]) && is_zero(wt[2]) && is_zero(wt[3])){
	  wt += 4;
	  continue;
	}
	++wt; i=*wt;
	if (is_zero(i)){
	  ++wt; i=*wt;
	  if (is_zero(i)){
	    ++wt; i=*wt;
	  }
	}
      }
      *wt = create<modt>(0);
      i %= env;
      if (is_zero(i)){
	wt++; continue;
      }
      unsigned I=unsigned(wt-wt0);
      *(uit+I)=1; // used[i]=1;
      bitmap[I>>5] |= (1<<(I&0x1f));
      lescoeffs.push_back(shrink(i));
      wt++;
    }
#endif
    for (;wt<wtend;++wt){
      modt i=*wt;
      if (is_zero(i)) continue;
      *wt=create<modt>(0);
      i %= env;
      if (is_zero(i)) continue;
      unsigned I=unsigned(wt-wt0);
      *(uit+I)=1; // used[i]=1;
      bitmap[I>>5] |= (1<<(I&0x1f));
      lescoeffs.push_back(shrink(i));
    }
    return res;
  }

#if 1 && defined PSEUDO_MOD && GIAC_SHORTSHIFTTYPE==16 && !defined BIGENDIAN
  inline void special_mod32(modint & a,int b,int c,int p,unsigned invp,unsigned nbits){
    a=pseudo_mod(a-((longlong)b)*c,p,invp,nbits);
  }

  void f4_innerloop_special_mod32(modint * wt,const modint * jt,const modint * jtend,modint C,const shifttype* it,modint env,unsigned invp,unsigned nbits){
    if (jtend-jt>3 && ((ulonglong) it &0x2)){ // align it address
      // should be always true (since we have already read one time)
      wt += *it; ++it;
      special_mod32(*wt,C,*jt,env,invp,nbits); ++jt;
    }
    jtend -= 16;
    for (;jt<=jtend;){
      unsigned B;
      unsigned * IT=(unsigned *) it;
      B=*IT; 
      wt += (B&0xffff); 
      special_mod32(*wt,C,*jt,env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[1],env,invp,nbits); 
      B=IT[1];
      wt += (B&0xffff); 
      special_mod32(*wt,C,jt[2],env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[3],env,invp,nbits); 
      B=IT[2];
      wt += (B&0xffff); 
      special_mod32(*wt,C,jt[4],env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[5],env,invp,nbits); 
      B=IT[3];
      wt += (B&0xffff); 
      special_mod32(*wt,C,jt[6],env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[7],env,invp,nbits); 
      B=IT[4];
      wt += (B&0xffff); 
      special_mod32(*wt,C,jt[8],env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[9],env,invp,nbits); 
      B=IT[5];
      wt += (B&0xffff); 
      special_mod32(*wt,C,jt[10],env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[11],env,invp,nbits); 
      B=IT[6];
      wt += (B&0xffff); 
      special_mod32(*wt,C,jt[12],env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[13],env,invp,nbits); 
      B=IT[7];
      wt += (B&0xffff); 
      special_mod32(*wt,C,jt[14],env,invp,nbits);
      wt += (B>>16);;
      special_mod32(*wt,C,jt[15],env,invp,nbits); 
      it += 16; jt+=16;
    }
    jtend += 16;
    for (;jt!=jtend;++jt){
      wt += *it; ++it;
      special_mod32(*wt,C,*jt,env,invp,nbits); 
    }
  }

  unsigned reducef4buchbergersplit32(vector<modint> &v32,const vector< vector<shifttype> > & M,const vector<unsigned> & firstpos,unsigned firstcol,const vector< vector<modint> > & coeffs,const vector<coeffindex_t> & coeffindex,vector<modint> & lescoeffs,unsigned * bitmap,vector<used_t> & used,modint env){
    vector<unsigned>::const_iterator fit=firstpos.begin(),fit0=fit,fitend=firstpos.end(),fit1=fit+firstcol,fit2;
    if (fit1>fitend)
      fit1=fitend;
    vector<modint>::iterator wt=v32.begin(),wt0=wt,wt1,wtend=v32.end();
    unsigned skip=0;
    while (fit+1<fit1){
      fit2=fit+(fit1-fit)/2;
      if (*fit2>firstcol)
	fit1=fit2;
      else
	fit=fit2;
    }
    if (debug_infolevel>2)
      CERR << "Firstcol " << firstcol << "/" << v32.size() << " ratio skipped " << (fit-fit0)/double(fitend-fit0) << '\n';
    int nbits=sizeinbase2(env);
    unsigned invp=((1ULL<<(2*nbits)))/env+1;
    for (;fit!=fitend;++fit){
      if (v32[*fit]==0)
        continue;
      unsigned i=unsigned(fit-fit0);
      const vector<shifttype> & mindex=M[i];
      const shifttype * it=&mindex.front();
      unsigned pos=0;
      next_index(pos,it);
      skip=pos;
      wt=wt0+pos;
      // if (*wt==0) continue;
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      bool shortshifts=coeffindex[i].b;
      if (mcoeff.empty())
        continue;
      const modint * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
      modint c=*wt % env; 
      if (c<0) c += env;
      *wt=0;
      if (!c)
        continue;
      ++jt;
      if (shortshifts){
        f4_innerloop_special_mod32(&*wt,jt,jtend,c,it,env,invp,nbits);
      }
      else {
        for (;jt<jt_;){
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits); // *wt-=extend(c)*(*jt);
          ++jt;
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
          ++jt;
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
          ++jt;
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
          ++jt;
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
          ++jt;
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
          ++jt;
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
          ++jt;
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
          ++jt;
        }
        for (;jt!=jtend;++jt){
          next_index(wt,it);
          special_mod32(*wt,c,*jt,env,invp,nbits);
        }
      } // end else shortshifts
    } // end for
    if (!bitmap)
      return 0; // result in v32, for multiple uses
    return store_coeffs(v32,firstcol,lescoeffs,bitmap,used,env);
  }
#endif

  unsigned reducef4buchbergersplit(vector<modint2> &v64,const vector< vector<shifttype> > & M,const vector<unsigned> & firstpos,unsigned firstcol,const vector< vector<modint> > & coeffs,const vector<coeffindex_t> & coeffindex,vector<modint> & lescoeffs,unsigned * bitmap,vector<used_t> & used,modint env){
    vector<unsigned>::const_iterator fit=firstpos.begin(),fit0=fit,fitend=firstpos.end(),fit1=fit+firstcol,fit2;
    if (fit1>fitend)
      fit1=fitend;
    vector<modint2>::iterator wt=v64.begin(),wt0=wt,wt1,wtend=v64.end();
    unsigned skip=0;
    while (fit+1<fit1){
      fit2=fit+(fit1-fit)/2;
      if (*fit2>firstcol)
	fit1=fit2;
      else
	fit=fit2;
    }
    if (debug_infolevel>2)
      CERR << "Firstcol " << firstcol << "/" << v64.size() << " ratio skipped " << (fit-fit0)/double(fitend-fit0) << '\n';
    if (env<(1<<24)){
#ifdef PSEUDO_MOD
      int nbits=sizeinbase2(env);
      unsigned invmodulo=((1ULL<<(2*nbits)))/env+1;
#endif
      bool fastcheck = (fitend-fit)<32768;
      unsigned redno=0;
      for (;fit<fitend;++fit){
	if (*(wt0+*fit)==0){
	  //if (fit<fit4 && *(wt0+fit[1])==0 && *(wt0+fit[2])==0 && *(wt0+fit[3])==0 ) fit += 3;
	  continue;
	}
	unsigned i=unsigned(fit-fit0);
	const vector<shifttype> & mindex=M[i];
	const shifttype * it=&mindex.front();
	unsigned pos=0;
	next_index(pos,it);
	skip=pos;
	wt=wt0+pos;
	//if (*wt==0) continue; // test already done with v64[*fit]==0
	const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
	bool shortshifts=coeffindex[i].b;
	if (mcoeff.empty())
	  continue;
	const modint * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
	// if (pos>v.size()) CERR << "error" <<'\n';
	// if (*jt!=1) CERR << "not normalized " << i << '\n';
#if 0 // def PSEUDO_MOD, does not work for cyclic8m
	modint c=pseudo_mod(*wt,env,invmodulo,nbits); // *jt should be 1
#else
	modint c=*wt % env; // (extend(*jt)*(*wt % env))%env;
#endif
	*wt=0;
	if (!c)
	  continue;
	if (!fastcheck){
	  ++redno;
	  if (redno==32768){
	    redno=0;
	    // reduce the line mod env
	    //CERR << "reduce line" << '\n';
	    for (vector<modint2>::iterator wt=v64.begin()+pos;wt!=wtend;++wt){
	      // 2^63-1-p*p*32768 where p:=prevprime(2^24)
	      modint2 tmp=*wt;
	      if (tmp>=3298534588415LL || tmp<=-3298534588415LL)
		*wt = tmp % env;
	      // if (*wt) *wt %= env; // does not work pseudo_mod(*wt,env,invmodulo,nbits);
	    }
	  }
	}
	++jt;
#ifdef GIAC_SHORTSHIFTTYPE
	if (shortshifts){
	  f4_innerloop(&*wt,jt,jtend,c,it);
	}
	else {
	  for (;jt<jt_;){
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	    ++jt;
	  }
	  for (;jt!=jtend;++jt){
	    next_index(wt,it);
	    *wt-=extend(c)*(*jt);
	  }
	}
#else // def GIAC_SHORTSHIFTTYPE
	for (;jt<jt_;){
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	}
	for (;jt!=jtend;++jt){
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	}
#endif // def GIAC_SHORTSHIFTTYPE
      }
    }  
    else { // env > 2^24
      modint2 env2=extend(env)*env;
      for (;fit!=fitend;++fit){
	if (v64[*fit]==0)
	  continue;
	unsigned i=unsigned(fit-fit0);
	const vector<shifttype> & mindex=M[i];
	const shifttype * it=&mindex.front();
	unsigned pos=0;
	next_index(pos,it);
	skip=pos;
	wt=wt0+pos;
	// if (*wt==0) continue;
	const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
	bool shortshifts=coeffindex[i].b;
	if (mcoeff.empty())
	  continue;
	const modint * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
	// if (pos>v.size()) CERR << "error" <<'\n';
	// if (*jt!=1) CERR << "not normalized" << '\n';
	modint c=*wt % env; // (extend(*jt)*(*wt % env))%env;
	if (c<0) c += env;
	*wt=0;
	if (!c)
	  continue;
	++jt;
#ifdef GIAC_SHORTSHIFTTYPE
	if (shortshifts){
	  f4_innerloop_special_mod(&*wt,jt,jtend,c,it,env);
	}
	else {
	  for (;jt<jt_;){
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2); // 	    *wt-=extend(c)*(*jt);
	    ++jt;
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	    ++jt;
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	    ++jt;
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	    ++jt;
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	    ++jt;
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	    ++jt;
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	    ++jt;
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	    ++jt;
	  }
	  for (;jt!=jtend;++jt){
	    next_index(wt,it);
	    special_mod(*wt,c,*jt,env,env2);
	  }
	}
#else // def GIAC_SHORTSHIFTTYPE
	for (;jt<jt_;){
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	}
	for (;jt!=jtend;++jt){
	  special_mod(v64[*it],c,*jt,env,env2);
	  ++it; ++jt;
	}
#endif // def GIAC_SHORTSHIFTTYPE
      }
    }
    if (!bitmap)
      return 0; // result in v64, for multiple uses
    return store_coeffs(v64,firstcol,lescoeffs,bitmap,used,env);
  }

#ifdef CPU_SIMD
  inline void special_mod(mod4int2 & x,const Vec4q & C,const mod4int & d,const Vec4q & P){
    Vec4q A;
    Vec4i D;
    D.load(&d); A.load(&x); 
    A -= C*extend(D); // or _mm256_mul_epi32 without extend?
    A += ((A>>63)&P);
    A.store(&x);
  }
#else
  inline void special_mod(mod4int2 & x,const mod4int2 & c,const mod4int & d,const mod4int2 & env2){
    mod4int2 y=x-c*d;
    x = y + ((y>>63)&env2);
  }
#endif

  void f4_innerloop_special_mod(
#ifdef CPU_SIMD
                                mod4int2 * wt,const mod4int * jt,const mod4int * jtend,const Vec4q & C,const shifttype* it,const Vec4q & P
#else
                                mod4int2 * wt,const mod4int * jt,const mod4int * jtend,const mod4int2 & C,const shifttype* it,const mod4int2 & P
#endif
                                ){
    if (jtend-jt>3 && ((ulonglong) it &0x2)){ // align it address
      // should be always true (since we have already read one time)
      wt += *it; ++it;
      special_mod(*wt,C,*jt,P); ++jt;
    }
    jtend -= 16;
#ifndef BIDGENDIAN  // it address is 32 bits aligned
    unsigned * IT=(unsigned *) it;
    for (;jt<=jtend;){
      unsigned B;
      B=*IT; //1+1read
      wt += (B&0xffff);  // 2
      special_mod(*wt,C,*jt,P); // 5+2read/1write
      wt += (B>>16); // 2
      special_mod(*wt,C,jt[1],P); //5+2R+1W
      B=IT[1]; // 1+1read
      wt += (B&0xffff); // 2
      special_mod(*wt,C,jt[2],P); // 5+2R+1W
      wt += (B>>16); // 2
      special_mod(*wt,C,jt[3],P);  //5+2R+1W => 30 instr + 10 reads + 4 write
      B=IT[2];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[4],P);
      wt += (B>>16);;
      special_mod(*wt,C,jt[5],P); 
      B=IT[3];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[6],P);
      wt += (B>>16);;
      special_mod(*wt,C,jt[7],P); 
      B=IT[4];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[8],P);
      wt += (B>>16);;
      special_mod(*wt,C,jt[9],P); 
      B=IT[5];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[10],P);
      wt += (B>>16);;
      special_mod(*wt,C,jt[11],P); 
      B=IT[6];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[12],P);
      wt += (B>>16);;
      special_mod(*wt,C,jt[13],P); 
      B=IT[7];
      wt += (B&0xffff); 
      special_mod(*wt,C,jt[14],P);
      wt += (B>>16);;
      special_mod(*wt,C,jt[15],P); 
      IT += 8; jt+=16;
    }
    it=(shifttype *) IT;
#else
    for (;jt<=jtend;){
      wt += it[0]; int b=it[1];
      special_mod(*wt,C,*jt,P); 
      special_mod(wt[b],C,jt[1],P); 
      wt += b+it[2]; b=it[3];
      special_mod(*wt,C,jt[2],P); 
      special_mod(wt[b],C,jt[3],P); 
      wt += b+it[4]; b=it[5];
      special_mod(*wt,C,jt[4],P); 
      special_mod(wt[b],C,jt[5],P); 
      wt += b+it[6]; b=it[7];
      special_mod(*wt,C,jt[6],P); 
      special_mod(wt[b],C,jt[7],P); 
      wt += b+it[8]; b=it[9];
      special_mod(*wt,C,jt[8],P); 
      special_mod(wt[b],C,jt[9],P); 
      wt += b+it[10]; b=it[11];
      special_mod(*wt,C,jt[10],P); 
      special_mod(wt[b],C,jt[11],P); 
      wt += b+it[12]; b=it[13];
      special_mod(*wt,C,jt[12],P); 
      special_mod(wt[b],C,jt[13],P); 
      wt += b+it[14]; b=it[15];
      special_mod(*wt,C,jt[14],P); 
      special_mod(wt[b],C,jt[15],P); 
      wt += b;
      it += 16; jt+=16;
    }
#endif
    jtend += 16;
    for (;jt!=jtend;++jt){
      wt += *it; ++it;
      special_mod(*wt,C,*jt,P); 
    }
  }

  unsigned reducef4buchbergersplit(vector<mod4int2> &v64,const vector< vector<shifttype> > & M,const vector<unsigned> & firstpos,unsigned firstcol,const vector< vector<mod4int> > & coeffs,const vector<coeffindex_t> & coeffindex,vector<mod4int> & lescoeffs,unsigned * bitmap,vector<used_t> & used,mod4int env){
    vector<unsigned>::const_iterator fit=firstpos.begin(),fit0=fit,fitend=firstpos.end(),fit1=fit+firstcol,fit2;
    if (fit1>fitend)
      fit1=fitend;
    vector<mod4int2>::iterator wt=v64.begin(),wt0=wt,wt1,wtend=v64.end();
    unsigned skip=0;
    while (fit+1<fit1){
      fit2=fit+(fit1-fit)/2;
      if (*fit2>firstcol)
	fit1=fit2;
      else
	fit=fit2;
    }
    if (debug_infolevel>2)
      CERR << "Firstcol " << firstcol << "/" << v64.size() << " ratio skipped " << (fit-fit0)/double(fitend-fit0) << '\n';
    mod4int2 env2=extend(env)*env;
    for (;fit!=fitend;++fit){
      if (is_zero(v64[*fit]))
        continue;
      unsigned i=unsigned(fit-fit0);
      const vector<shifttype> & mindex=M[i];
      const shifttype * it=&mindex.front();
      unsigned pos=0;
      next_index(pos,it);
      skip=pos;
      wt=wt0+pos;
      // if (*wt==0) continue;
      const vector<mod4int> & mcoeff=coeffs[coeffindex[i].u];
      bool shortshifts=coeffindex[i].b;
      if (mcoeff.empty())
        continue;
      const mod4int * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
      // if (pos>v.size()) CERR << "error" <<'\n';
      // if (*jt!=1) CERR << "not normalized" << '\n';
      mod4int c=*wt % env; // (extend(*jt)*(*wt % env))%env;
      c=makepositive(c,env); // if (c<0) c += env;
      mod4int2 cc=extend(c);
#ifdef CPU_SIMD
      Vec4q C; C.load(&cc);
      Vec4q P; P.load(&env2);
#else
      mod4int2 & C=cc;
      mod4int2 & P=env2;
#endif
      *wt=create<mod4int2>(0);
      if (is_zero(c))
        continue;
      ++jt;      
      if (shortshifts){
        f4_innerloop_special_mod(&*wt,jt,jtend,C,it,P);
      }
      else {
        for (;jt<jt_;){
          next_index(wt,it);
          special_mod(*wt,C,*jt,P); // 	    *wt-=extend(c)*(*jt);
          ++jt;
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
          ++jt;
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
          ++jt;
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
          ++jt;
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
          ++jt;
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
          ++jt;
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
          ++jt;
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
          ++jt;
        }
        for (;jt!=jtend;++jt){
          next_index(wt,it);
          special_mod(*wt,C,*jt,P);
        }
      }
    }
    if (!bitmap)
      return 0; // result in v64, for multiple uses
    return store_coeffs(v64,firstcol,lescoeffs,bitmap,used,env);
  }

  unsigned reducef4buchbergersplitdouble(vector<double> &v64,const vector< vector<shifttype> > & M,const vector<unsigned> & firstpos,unsigned firstcol,const vector< vector<modint> > & coeffs,const vector<coeffindex_t> & coeffindex,vector<modint> & lescoeffs,unsigned * bitmap,vector<used_t> & used,modint env){
    vector<unsigned>::const_iterator fit=firstpos.begin(),fit0=fit,fitend=firstpos.end(),fit1=fit+firstcol,fit2;
    if (fit1>fitend)
      fit1=fitend;
    vector<double>::iterator wt=v64.begin(),wt0=wt,wt1,wtend=v64.end();
    unsigned skip=0;
    while (fit+1<fit1){
      fit2=fit+(fit1-fit)/2;
      if (*fit2>firstcol)
	fit1=fit2;
      else
	fit=fit2;
    }
    if (debug_infolevel>2)
      CERR << "Firstcol " << firstcol << "/" << v64.size() << " ratio skipped " << (fit-fit0)/double(fitend-fit0) << '\n';
    double env2=double(env)*env;
    for (;fit!=fitend;++fit){
      if (v64[*fit]==0)
	continue;
      unsigned i=unsigned(fit-fit0);
      const vector<shifttype> & mindex=M[i];
      const shifttype * it=&mindex.front();
      unsigned pos=0;
      next_index(pos,it);
      skip=pos;
      wt=wt0+pos;
      // if (*wt==0) continue;
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      bool shortshifts=coeffindex[i].b;
      if (mcoeff.empty())
	continue;
      const modint * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
      // if (pos>v.size()) CERR << "error" <<'\n';
      // if (*jt!=1) CERR << "not normalized" << '\n';
      modint c=longlong(*wt) % env; // (extend(*jt)*(*wt % env))%env;
      if (c<0) c += env;
      *wt=0;
      if (!c)
	continue;
      ++jt;
#ifdef GIAC_SHORTSHIFTTYPE
      if (shortshifts){
	f4_innerloop_special_mod(&*wt,jt,jtend,c,it,env);
      }
      else {
	for (;jt<jt_;){
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2); // 	    *wt-=extend(c)*(*jt);
	  ++jt;
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	  ++jt;
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	  ++jt;
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	  ++jt;
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	  ++jt;
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	  ++jt;
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	  ++jt;
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	  ++jt;
	}
	for (;jt!=jtend;++jt){
	  next_index(wt,it);
	  special_mod(*wt,c,*jt,env,env2);
	}
      }
#else // def GIAC_SHORTSHIFTTYPE
      for (;jt<jt_;){
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
      }
      for (;jt!=jtend;++jt){
	special_mod(v64[*it],c,*jt,env,env2);
	++it; ++jt;
      }
#endif // def GIAC_SHORTSHIFTTYPE
    }
    if (!bitmap)
      return 0; // result in v64, for multiple uses
    unsigned res=0;
    used_t * uit=&used.front();
    wt=v64.begin()+firstcol;
    wt1=wtend-4;
#if 1
    for (;wt<=wt1;++wt){
      double i=*wt;
      if (i) break;
      ++wt; i=*wt;
      if (i) break;
      ++wt; i=*wt;
      if (i) break;
      ++wt; i=*wt;
      if (i) break;
    }
#endif
    if (!res){
      for (;wt<wtend;++wt){
	modint2 i=*wt;
	if (!i) continue;
	*wt = 0;
	i %= env;
	if (!i) continue;
	unsigned I=unsigned(wt-wt0);
	res=I;
	*(uit+I)=1; // used[i]=1;
	bitmap[I>>5] |= (1<<(I&0x1f));
	lescoeffs.push_back(modint(i));
	break;
      }
      if (!res)
	res=unsigned(v64.size());
    }
#if 1
    for (;wt<=wt1;++wt){
      modint2 i=*wt;
      if (!i){
	++wt; i=*wt;
	if (!i){
	  ++wt; i=*wt;
	  if (!i){
	    ++wt; i=*wt;
	    if (!i)
	      continue;
	  }
	}
      }
      *wt = 0;
      i %= env;
      if (!i) continue;
      unsigned I=unsigned(wt-wt0);
      *(uit+I)=1; // used[i]=1;
      bitmap[I>>5] |= (1<<(I&0x1f));
      lescoeffs.push_back(modint(i));
    }
#endif
    for (;wt<wtend;++wt){
      modint2 i=*wt;
      if (!i) continue;
      *wt=0;
      i %= env;
      if (!i) continue;
      unsigned I=unsigned(wt-wt0);
      *(uit+I)=1; // used[i]=1;
      bitmap[I>>5] |= (1<<(I&0x1f));
      lescoeffs.push_back(modint(i));
    }
    return res;
  }

  unsigned reducef4buchbergersplitu(vector<modint> &v,const vector< vector<unsigned> > & M,vector< vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env,vector<modint2> & v64){
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    if (env<(1<<24)){
      v64.resize(v.size());
      vector<modint2>::iterator wt=v64.begin(),wtend=v64.end();
      for (;vt!=vtend;++wt,++vt)
	*wt=*vt;
      for (unsigned i=0;i<M.size();++i){
	if ((i&0xffff)==0xffff){
	  // reduce the line mod env
	  for (wt=v64.begin();wt!=wtend;++wt){
	    if (*wt)
	      *wt %= env;
	  }
	}
	const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
	if (mcoeff.empty())
	  continue;
	const modint * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
	const vector<unsigned> & mindex=M[i];
	const unsigned * it=&mindex.front();
	unsigned pos=*it;
	// if (pos>v.size()) CERR << "error" <<'\n';
	// if (*jt!=1) CERR << "not normalized" << '\n';
	modint c=(extend(invmod(*jt,env))*(v64[pos] % env))%env;
	v64[pos]=0;
	if (!c)
	  continue;
	++it; ++jt;
	for (;jt<jt_;){
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	}
	for (;jt!=jtend;++it,++jt){
	  v64[*it]-=extend(c)*(*jt);
	}
      }
      for (vt=v.begin(),wt=v64.begin();vt!=vtend;++wt,++vt){
	if (*wt)
	  *vt = *wt % env;
	else
	  *vt=0;
      }
    }
    else { // large modulo
#ifdef PSEUDO_MOD
      int nbits=sizeinbase2(env);
      unsigned invmodulo=((1ULL<<(2*nbits)))/env+1;
#endif
      for (unsigned i=0;i<M.size();++i){
	const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
	vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end();
	if (jt==jtend)
	  continue;
	const vector<unsigned> & mindex=M[i];
	const unsigned * it=&mindex.front();
	unsigned pos=*it; 
	// if (pos>v.size()) CERR << "error" <<'\n';
	modint c=(extend(invmod(*jt,env))*v[pos])%env;
	v[pos]=0;
	if (!c)
	  continue;
	++it; ++jt;
#ifdef PSEUDO_MOD
	if (env<(1<<29)){
	  c=-c;
	  for (;jt!=jtend;++jt){
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(v[*it],c,*jt,env,invmodulo,nbits);
	    ++it;
	  }
	  continue;
	}
#endif
	for (;jt!=jtend;++jt){
	  modint &x=v[*it];
	  ++it;
	  x=(x-extend(c)*(*jt))%env;
	}
      }
      vector<modint>::iterator vt=v.begin(),vtend=v.end();
#ifdef PSEUDO_MOD
      for (vt=v.begin();vt!=vtend;++vt){
	if (*vt)
	  *vt %= env;
      }
#endif
    } // end else based on modulo size
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return unsigned(vt-v.begin());
    }
    return unsigned(v.size());
  }

  unsigned reducef4buchbergersplits(vector<modint> &v,const vector< vector<unsigned short> > & M,vector< vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env,vector<modint2> & v64){
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    if (env<(1<<24)){
      v64.resize(v.size());
      vector<modint2>::iterator wt=v64.begin(),wtend=v64.end();
      for (;vt!=vtend;++wt,++vt)
	*wt=*vt;
      for (unsigned i=0;i<M.size();++i){
	if ((i&0xffff)==0xffff){
	  // reduce the line mod env
	  for (wt=v64.begin();wt!=wtend;++wt){
	    if (*wt)
	      *wt %= env;
	  }
	}
	const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
	if (mcoeff.empty())
	  continue;
	const modint * jt=&mcoeff.front(),*jtend=jt+mcoeff.size(),*jt_=jtend-8;
	const vector<unsigned short> & mindex=M[i];
	const unsigned short * it=&mindex.front();
	unsigned pos=*it;
	// if (pos>v.size()) CERR << "error" <<'\n';
	// if (*jt!=1) CERR << "not normalized" << '\n';
	modint c=(extend(invmod(*jt,env))*(v64[pos] % env))%env;
	v64[pos]=0;
	if (!c)
	  continue;
	++it; ++jt;
	for (;jt<jt_;){
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	  v64[*it]-=extend(c)*(*jt);
	  ++it; ++jt;
	}
	for (;jt!=jtend;++it,++jt){
	  v64[*it]-=extend(c)*(*jt);
	}
      }
      for (vt=v.begin(),wt=v64.begin();vt!=vtend;++wt,++vt){
	if (*wt)
	  *vt = *wt % env;
	else
	  *vt=0;
      }
    }
    else { // large modulo
#ifdef PSEUDO_MOD
      int nbits=sizeinbase2(env);
      unsigned invmodulo=((1ULL<<(2*nbits)))/env+1;
#endif
      for (unsigned i=0;i<M.size();++i){
	const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
	vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end();
	if (jt==jtend)
	  continue;
	const vector<unsigned short> & mindex=M[i];
	const unsigned short * it=&mindex.front();
	unsigned pos=*it; 
	// if (pos>v.size()) CERR << "error" <<'\n';
	modint c=(extend(invmod(*jt,env))*v[pos])%env;
	v[pos]=0;
	if (!c)
	  continue;
	++it; ++jt;
#ifdef PSEUDO_MOD
	if (env<(1<<29)){
	  c=-c;
	  for (;jt!=jtend;++jt){
	    // if (pos>v.size()) CERR << "error" <<'\n';
	    pseudo_mod(v[*it],c,*jt,env,invmodulo,nbits);
	    ++it;
	  }
	  continue;
	}
#endif
	for (;jt!=jtend;++jt){
	  modint &x=v[*it];
	  ++it;
	  x=(x-extend(c)*(*jt))%env;
	}
      }
      vector<modint>::iterator vt=v.begin(),vtend=v.end();
#ifdef PSEUDO_MOD
      for (vt=v.begin();vt!=vtend;++vt){
	if (*vt)
	  *vt %= env;
      }
#endif
    } // end else based on modulo size
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return unsigned(vt-v.begin());
    }
    return unsigned(v.size());
  }

  bool tri(const vector<sparse_element> & v1,const vector<sparse_element> & v2){
    return v1.front().pos<v2.front().pos;
  }

  struct sparse_element_tri1 {
    sparse_element_tri1(){}
    bool operator() (const sparse_element & v1,const sparse_element & v2){
      return v1.val<v2.val;
    }
  };

  void sort_vector_sparse_element(vector<sparse_element>::iterator it,vector<sparse_element>::iterator itend){
    sort(it,itend,sparse_element_tri1());
  }
  
  // if sorting with presumed size, adding reconstructed generators will
  // not work...
  template <class poly> 
  struct tripolymod_tri {
    int sort_by_logz_age;
    tripolymod_tri(int b):sort_by_logz_age(b){}
    bool operator() (const poly & v1,const poly & v2){
      if (sort_by_logz_age==1 && v1.logz!=v2.logz)
	return v1.logz<v2.logz;
      if (sort_by_logz_age==2 && v1.age!=v2.age)
	return v1.age<v2.age;
      return tdeg_t_strictly_greater(v2.coord.front().u,v1.coord.front().u,v1.order);
    }
  };

  template<class tdeg_t,class modint_t>
  void makeline(const polymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<modint_t> & v,int start=0){
    v.resize(R.coord.size()); 
    v.assign(R.coord.size(),0);
    typename std::vector< T_unsigned<modint,tdeg_t> >::const_iterator it=p.coord.begin()+start,itend=p.coord.end(),jt=R.coord.begin(),jtbeg=jt,jtend=R.coord.end();
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v[jt-jtbeg]=it->g;
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v[jt-jtbeg]=it->g;
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void makelinesub(const polymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<modint_t> & v,int start,modint_t env){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin()+start,itend=p.coord.end(),jt=R.coord.begin(),jtbeg=jt,jtend=R.coord.end();
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    // v[jt-jtbeg] -= it->g;
	    modint_t & vv=v[jt-jtbeg];
	    vv = (vv-extend(it->g))%env;
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    // v[jt-jtbeg]-=it->g;
	    modint_t & vv=v[jt-jtbeg];
	    vv = (vv-extend(it->g))%env;
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  // put in v coeffs of polymod corresponding to R, and in rem those who do not match
  // returns false if v is null
  template<class tdeg_t,class modint_t>
  bool makelinerem(const polymod<tdeg_t,modint_t> & p,polymod<tdeg_t,modint_t> & rem,const polymod<tdeg_t,modint_t> & R,vector<modint_t> & v){
    rem.coord.clear();
    v.clear();
    v.resize(R.coord.size()); 
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=R.coord.begin(),jtend=R.coord.end();
    bool res=false;
    for (;it!=itend;++it){
      const tdeg_t & u=it->u;
      for (;jt!=jtend;++jt){
	if (tdeg_t_greater(u,jt->u,p.order) 
	    // u>=jt->u
	    ){
	  if (u==jt->u){
	    res=true;
	    v[jt-R.coord.begin()]=it->g;
	    ++jt;
	  }
	  else
	    rem.coord.push_back(*it);
	  break;
	}
      }
    }
    return res;
  }

  template<class tdeg_t,class modint_t>
  void makeline(const polymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<sparse_element> & v){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=R.coord.begin(),jtend=R.coord.end();
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v.push_back(sparse_element(it->g,jt-R.coord.begin()));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v.push_back(sparse_element(it->g,jt-R.coord.begin()));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  
#if 1
  void convert(const vector<modint> & v,vector<sparse_element> & w,vector<used_t> & used){
    unsigned count=0;
    vector<modint>::const_iterator it=v.begin(),itend=v.end();
    vector<used_t>::iterator ut=used.begin();
    for (;it!=itend;++ut,++it){
      if (!*it)
	continue;
      *ut=1;
      ++count;
    }
    w.clear();
    w.reserve(count);
    for (count=0,it=v.begin();it!=itend;++count,++it){
      if (*it)
	w.push_back(sparse_element(*it,count));
    }
  }

#else
  void convert(const vector<modint> & v,vector<sparse_element> & w,vector<used_t> & used){
    unsigned count=0;
    vector<modint>::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (*it){
#if 1
	used[it-v.begin()]=1;
#else
	++used[it-v.begin()];
	if (used[it-v.begin()]>100)
	  used[it-v.begin()]=100;
#endif
	++count;
      }
    }
    w.clear();
    w.reserve(count);
    for (it=v.begin();it!=itend;++it){
      if (*it)
	w.push_back(sparse_element(*it,it-v.begin()));
    }
  }
#endif

  // add to w non-zero coeffs of v, set bit i in bitmap to 1 if v[i]!=0
  // bitmap size is rounded to a multiple of 32
  void zconvert(const vector<modint> & v,vector<modint>::iterator & coeffit,unsigned * bitmap,vector<used_t> & used){
    vector<modint>::const_iterator it=v.begin(),itend=v.end();
    used_t * uit=&used.front();
    for (unsigned i=0;it!=itend;++i,++it){
      if (!*it)
	continue;
      *(uit+i)=1; // used[i]=1;
      bitmap[i>>5] |= (1<<(i&0x1f));
      *coeffit=*it;
      ++coeffit;
    }
  }

  template<class modint_t>
  void zconvert_(vector<modint_t> & v,vector<modint_t> & lescoeffs,unsigned * bitmap,vector<used_t> & used){
    typename vector<modint_t>::iterator it0=v.begin(),it=it0,itend=v.end(),itend4=itend-4;
    used_t * uit=&used.front();
    for (;it<=itend4;++it){
      if (!*it ){
	++it;
	if (!*it){
	  ++it;
	  if (!*it){
	    ++it;
	    if (!*it)
	      continue;
	  }
	}
      }
      unsigned i=unsigned(it-it0);
      *(uit+i)=1; // used[i]=1;
      bitmap[i>>5] |= (1<<(i&0x1f));
      lescoeffs.push_back(*it);
      *it=0;
    }
    for (;it!=itend;++it){
      if (!*it)
	continue;
      unsigned i=unsigned(it-it0);
      *(uit+i)=1; // used[i]=1;
      bitmap[i>>5] |= (1<<(i&0x1f));
      lescoeffs.push_back(*it);
      *it=0;
    }
  }

  // create matrix from list of coefficients and bitmap of non-zero positions
  // M must already have been created with the right number of rows
  template<class modint_t>
  void create_matrix(const vector<modint_t> & lescoeffs,const unsigned * bitmap,unsigned bitmapcols,const vector<used_t> & used,vector< vector<modint_t> > & M){
    unsigned nrows=unsigned(M.size());
    int ncols=0;
    vector<used_t>::const_iterator ut=used.begin(),utend=used.end();
    unsigned jend=unsigned(utend-ut);
    typename vector<modint_t>::const_iterator it=lescoeffs.begin();
    for (;ut!=utend;++ut){
      ncols += *ut;
    }
    // do all memory allocation at once, trying to speed up threaded execution
    for (unsigned i=0;i<nrows;++i)
      M[i].resize(ncols);
    for (unsigned i=0;i<nrows;++i){
      const unsigned * bitmapi = bitmap + i*bitmapcols;
      typename vector<modint_t>::iterator mi=M[i].begin();
      unsigned j=0;
      for (;j<jend;++j){
	if (!used[j])
	  continue;
	if (bitmapi[j>>5] & (1<<(j&0x1f))){
	  *mi=*it;
	  ++it;
	}
	++mi;
      }
    }
  }

  template<class modint_t>
  unsigned create_matrix(const unsigned * bitmap,unsigned bitmapcols,const vector<used_t> & used,vector< vector<modint_t> > & M){
    unsigned nrows=unsigned(M.size()),zeros=0;
    int ncols=0;
    vector<used_t>::const_iterator ut=used.begin(),utend=used.end();
    unsigned jend=unsigned(utend-ut);
    for (;ut!=utend;++ut){
      ncols += *ut;
    }
    vector<modint_t> tmp;
    for (unsigned i=0;i<nrows;++i){
      if (M[i].empty()){ ++zeros; continue; }
      const unsigned * bitmapi = bitmap + i*bitmapcols;
      tmp.clear();
      tmp.resize(ncols);
      tmp.swap(M[i]);
      typename vector<modint_t>::iterator mi=M[i].begin(),it=tmp.begin();
      unsigned j=0;
      for (;j<jend;++j){
	if (!used[j])
	  continue;
	if (bitmapi[j>>5] & (1<<(j&0x1f))){
	  *mi=*it;
	  ++it;
	}
	++mi;
      }
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " " << zeros << " null lines over " << M.size() << '\n';
    return zeros;
  }

  inline void push32(vector<sparse32> & v,modint val,unsigned & pos,unsigned newpos){
    unsigned shift=newpos-pos;
    if (newpos && (shift <(1<<7)))
      v.push_back(sparse32(val,shift));
    else {
      v.push_back(sparse32(val,0));
      v.push_back(sparse32());
      * (unsigned *) & v.back() =newpos;
    }
    pos=newpos;
  }

  template <class tdeg_t,class modint_t>
  void makeline32(const polymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<sparse32> & v){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=R.coord.begin(),jtend=R.coord.end();
    unsigned pos=0;
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    push32(v,it->g,pos,unsigned(jt-R.coord.begin()));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    push32(v,it->g,pos,unsigned(jt-R.coord.begin()));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  void convert32(const vector<modint> & v,vector<sparse32> & w,vector<used_t> & used){
    unsigned count=0;
    vector<modint>::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (*it){
#if 1
	used[it-v.begin()]=1;
#else
	++used[it-v.begin()];
	if (used[it-v.begin()]>100)
	  used[it-v.begin()]=100;
#endif
	++count;
      }
    }
    w.clear();
    w.reserve(1+int(count*1.1));
    unsigned pos=0;
    for (it=v.begin();it!=itend;++it){
      if (*it)
	push32(w,*it,pos,unsigned(it-v.begin()));
    }
  }

template<class tdeg_t,class modint_t,class modint_t2>
  void rref_f4buchbergermod_interreduce(vectpolymod<tdeg_t,modint_t> & f4buchbergerv,const vector<unsigned> & f4buchbergervG,vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,unsigned excluded,const vectpolymod<tdeg_t,modint_t> & quo,const polymod<tdeg_t,modint_t> & R,modint_t env,vector<int> & permutation){
    // step2: for each monomials of quo[i], shift res[G[i]] by monomial
    // set coefficient in a line of a matrix M, columns are R monomials indices
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin build M" << '\n';
    unsigned N=unsigned(R.coord.size()),i,j=0;
    unsigned c=N;
    double sknon0=0;
    vector<used_t> used(N,0);
    unsigned usedcount=0,zerolines=0,Msize=0;
    vector< vector<modint_t> > K(f4buchbergervG.size());
    for (i=0;i<G.size();++i){
      Msize += unsigned(quo[i].coord.size());
    }
    if ( env<(1<<24) && env*double(env)*Msize<9.223e18 ){
      vector< vector<sparse32> > M;
      M.reserve(N);
      vector<sparse_element> atrier;
      atrier.reserve(N);
      for (i=0;i<G.size();++i){
	typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
	for (;jt!=jtend;++j,++jt){
	  M.push_back(vector<sparse32>(0));
	  M[j].reserve(1+int(1.1*res[G[i]].coord.size()));
	  makeline32(res[G[i]],&jt->u,R,M[j]);
	  // CERR << M[j] << '\n';
	  if (M[j].front().shift)
	    atrier.push_back(sparse_element(M[j].front().shift,j));
	  else
	    atrier.push_back(sparse_element(*(unsigned *) &M[j][1],j));
	}
      }
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " end build M32" << '\n';
      // should not sort but compare res[G[i]]*quo[i] monomials to build M already sorted
      // CERR << "before sort " << M << '\n';
      sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1);
      vector< vector<sparse32> > M1(atrier.size());
      double mem=0; // mem*4=number of bytes allocated for M1
      for (i=0;i<atrier.size();++i){
	swap(M1[i],M[atrier[i].pos]);
	mem += M1[i].size();
      }
      swap(M,M1);
      bool freemem=mem>4e7; // should depend on real memory available
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " M32 sorted, rows " << M.size() << " columns " << N << " terms " << mem << " ratio " << (mem/M.size())/N <<'\n';
      // CERR << "after sort " << M << '\n';
      // step3 reduce
      vector<modint_t> v(N); vector<modint_t2> w(N);
      vector< vector<sparse32> > SK(f4buchbergerv.size());
      for (i=0;i<f4buchbergervG.size();++i){
	if (!f4buchbergerv[f4buchbergervG[i]].coord.empty()){
	  makeline<tdeg_t>(f4buchbergerv[f4buchbergervG[i]],0,R,v);
	  if (freemem){ 
	    polymod<tdeg_t,modint_t> clearer; swap(f4buchbergerv[f4buchbergervG[i]].coord,clearer.coord); 
	  }
	  c=giacmin(c,reducef4buchberger_32(v,M,env,w));
	  // convert v to a sparse vector in SK and update used
	  convert32(v,SK[i],used);
	  //CERR << v << '\n' << SK[i] << '\n';
	}
      }
      M.clear();
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " f4buchbergerv reduced " << f4buchbergervG.size() << " polynoms over " << N << " monomials, start at " << c << '\n';
      for (i=0;i<N;++i)
	usedcount += (used[i]>0);
      if (debug_infolevel>1){
	CERR << CLOCK()*1e-6 << " number of non-zero columns " << usedcount << " over " << N << '\n'; // usedcount should be approx N-M.size()=number of cols of M-number of rows
	if (debug_infolevel>2)
	  CERR << " column32 used " << used << '\n';
      }
      // create dense matrix K 
      for (i=0; i<K.size(); ++i){
	vector<modint_t> & v =K[i];
	if (SK[i].empty()){
	  ++zerolines;
	  continue;
	}
	v.resize(usedcount);
	typename vector<modint_t>::iterator vt=v.begin();
	vector<used_t>::const_iterator ut=used.begin(),ut0=ut;
	vector<sparse32>::const_iterator st=SK[i].begin(),stend=SK[i].end();
	unsigned p=0;
	for (j=0;st!=stend;++j,++ut){
	  if (!*ut) 
	    continue;
	  if (st->shift){
	    if (j==p + st->shift){
	      p += st->shift;
	      *vt=st->val;
	      ++st;
	      ++sknon0;
	    }
	  }
	  else {
	    if (j==* (unsigned *) &(*(st+1))){
	      *vt=st->val;
	      ++st;
	      p = * (unsigned *) &(*st);
	      ++st;
	      ++sknon0;
	    }
	  }
	  ++vt;
	}
#if 0
	vector<sparse32> clearer;
	swap(SK[i],clearer); // clear SK[i] memory
#endif
      }
    }
    else {
      vector< vector<sparse_element> > M;
      M.reserve(N);
      vector<sparse_element> atrier;
      atrier.reserve(N);
      for (i=0;i<G.size();++i){
	typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
	for (;jt!=jtend;++j,++jt){
	  M.push_back(vector<sparse_element>(0));
	  M[j].reserve(res[G[i]].coord.size());
	  makeline(res[G[i]],&jt->u,R,M[j]);
	  atrier.push_back(sparse_element(M[j].front().pos,j));
	}
      }
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " end build M" << '\n';
      // should not sort but compare res[G[i]]*quo[i] monomials to build M already sorted
      // CERR << "before sort " << M << '\n';
      sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1); 
      vector< vector<sparse_element> > M1(atrier.size());
      double mem=0; // mem*8=number of bytes allocated for M1
      unsigned firstpart=0;
      for (i=0;i<atrier.size();++i){
	swap(M1[i],M[atrier[i].pos]);
	mem += M1[i].size();
	if (!M1[i].empty())
	  firstpart=giacmax(firstpart,M1[i].front().pos);
      }
      swap(M,M1);
      bool freemem=mem>4e7; // should depend on real memory available
      // sort(M.begin(),M.end(),tri);
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " M sorted, rows " << M.size() << " columns " << N << "[" << firstpart << "] terms " << mem << " ratio " << (mem/N)/M.size() << '\n';
      // CERR << "after sort " << M << '\n';
      // step3 reduce
      vector<modint_t> v(N);
      vector< vector<sparse_element> > SK(f4buchbergerv.size());
#ifdef x86_64
      vector<int128_t> v128(N); 
      vector<modint_t> multiplier(M.size()); vector<unsigned> pos(M.size());
#endif
      for (i=0;i<f4buchbergervG.size();++i){
	if (!f4buchbergerv[f4buchbergervG[i]].coord.empty()){
	  makeline<tdeg_t>(f4buchbergerv[f4buchbergervG[i]],0,R,v);
	  if (freemem){ 
	    polymod<tdeg_t,modint_t> clearer; swap(f4buchbergerv[f4buchbergervG[i]].coord,clearer.coord); 
	  }
#ifdef x86_64
	  /* vector<modint> w(v);
	  // CERR << "reduce " << v << '\n' << M << '\n';
	  c=giacmin(c,reducef4buchbergerslice(w,M,env,v128,multiplier,pos));
	  c=giacmin(c,reducef4buchberger_64(v,M,env,v128));
	  if (w!=v) CERR << w << '\n' << v << '\n'; else CERR << "ok" << '\n';
	  */
	  // c=giacmin(c,reducef4buchbergerslice(v,M,env,v128,multiplier,pos));
	  if (0 && env<(1<<29) && N>10000) // it's slower despite v128 not in cache
	    c=giacmin(c,reducef4buchberger(v,M,env));
	  else
	    c=giacmin(c,reducef4buchberger_64(v,M,env,v128));
#else // x86_64
	  c=giacmin(c,reducef4buchberger(v,M,env));
#endif // x86_64
	  // convert v to a sparse vector in SK and update used
	  convert(v,SK[i],used);
	  // CERR << v << '\n' << SK[i] << '\n';
	}
      }
      M.clear();
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " f4buchbergerv reduced " << f4buchbergervG.size() << " polynoms over " << N << " monomials, start at " << c << '\n';
      for (i=0;i<N;++i)
	usedcount += (used[i]>0);
      if (debug_infolevel>1){
	CERR << CLOCK()*1e-6 << " number of non-zero columns " << usedcount << " over " << N << '\n'; // usedcount should be approx N-M.size()=number of cols of M-number of rows
	// if (debug_infolevel>2) CERR << " column use " << used << '\n';
      }
      // create dense matrix K 
      for (i=0; i<K.size(); ++i){
	vector<modint_t> & v =K[i];
	if (SK[i].empty()){
	  ++zerolines;
	  continue;
	}
	sknon0 += SK[i].size();
	v.resize(usedcount);
	typename vector<modint_t>::iterator vt=v.begin();
	vector<used_t>::const_iterator ut=used.begin(),ut0=ut;
	vector<sparse_element>::const_iterator st=SK[i].begin(),stend=SK[i].end();
	for (j=0;st!=stend;++j,++ut){
	  if (!*ut) 
	    continue;
	  if (j==st->pos){
	    *vt=st->val;
	    ++st;
	  }
	  ++vt;
	}
#if 1
	vector<sparse_element> clearer;
	swap(SK[i],clearer); // clear SK[i] memory
#endif
	// CERR << used << '\n' << SK[i] << '\n' << K[i] << '\n';
      }
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " rref " << K.size() << "x" << usedcount << " non0 " << sknon0 << " ratio " << (sknon0/K.size())/usedcount << " nulllines " << zerolines << '\n';
    vecteur pivots; vector<int> maxrankcols; longlong idet;
    // CERR << K << '\n';
    smallmodrref(1,K,pivots,permutation,maxrankcols,idet,0,int(K.size()),0,usedcount,1/* fullreduction*/,0/*dontswapbelow*/,env,0/* rrefordetorlu*/,true,0,true,-1);
    //CERR << K << "," << permutation << '\n';
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=R.coord.begin(),itend=R.coord.end();
    vector<int> permu=perminv(permutation);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv interreduced" << '\n';
    for (i=0;i<f4buchbergervG.size();++i){
#if 0 // spare memory, keep exactly the right number of monomials in f4buchbergerv[]
      polymod<tdeg_t,modint_t> tmpP(f4buchbergerv[f4buchbergervG[i]].order,f4buchbergerv[f4buchbergervG[i]].dim);
      vector<modint_t> & v =K[permu[i]];
      if (v.empty())
	continue;
      unsigned vcount=0;
      vector<modint_t>::const_iterator vt=v.begin(),vtend=v.end();
      for (;vt!=vtend;++vt){
	if (*vt)
	  ++vcount;
      }
      vector< T_unsigned<modint_t,tdeg_t> > & Pcoord=tmpP.coord;
      Pcoord.reserve(vcount);
      vector<used_t>::const_iterator ut=used.begin();
      for (vt=v.begin(),it=R.coord.begin();it!=itend;++ut,++it){
	if (!*ut)
	  continue;
	modint_t coeff=*vt;
	++vt;
	if (coeff!=0)
	  Pcoord.push_back(T_unsigned<modint_t,tdeg_t>(coeff,it->u));
      }
      if (!Pcoord.empty() && Pcoord.front().g!=1){
	smallmultmod(invmod(Pcoord.front().g,env),tmpP,env);	
	Pcoord.front().g=1;
      }
      swap(tmpP.coord,f4buchbergerv[f4buchbergervG[i]].coord);
#else
      // CERR << v << '\n';
      vector< T_unsigned<modint_t,tdeg_t> > & Pcoord=f4buchbergerv[f4buchbergervG[i]].coord;
      Pcoord.clear();
      vector<modint_t> & v =K[permu[i]];
      if (v.empty())
	continue;
      unsigned vcount=0;
      typename vector<modint_t>::const_iterator vt=v.begin(),vtend=v.end();
      for (;vt!=vtend;++vt){
	if (*vt)
	  ++vcount;
      }
      Pcoord.reserve(vcount);
      vector<used_t>::const_iterator ut=used.begin();
      for (vt=v.begin(),it=R.coord.begin();it!=itend;++ut,++it){
	if (!*ut)
	  continue;
	modint_t coeff=*vt;
	++vt;
	if (coeff!=0)
	  Pcoord.push_back(T_unsigned<modint_t,tdeg_t>(coeff,it->u));
      }
      if (!Pcoord.empty() && Pcoord.front().g!=1){
	smallmultmod(invmod(Pcoord.front().g,env),f4buchbergerv[f4buchbergervG[i]],env);	
	Pcoord.front().g=1;
      }
#endif
    }
  }


  template<class tdeg_t,class modint_t>
  void copycoeff(const polymod<tdeg_t,modint_t> & p,vector<modint_t> & v){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    v.clear();
    v.reserve(itend-it);
    for (;it!=itend;++it)
      v.push_back(it->g);
  }

  template<class tdeg_t,class modint_t>
  void copycoeff(const poly8<tdeg_t> & p,vector<gen> & v){
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    v.clear();
    v.reserve(itend-it);
    for (;it!=itend;++it)
      v.push_back(it->g);
  }

  // dichotomic seach for jt->u==u in [jt,jtend[
  template <class tdeg_t,class modint_t>
  bool dicho(typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator & jt,typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jtend,const tdeg_t & u,order_t order){
    if (jt->u==u) return true;
    for (;;){
      int step=int((jtend-jt)/2);
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator j=jt+step;
      if (j==jt)
	return j->u==u;
      //PREFETCH(&*(j+step/2));
      //PREFETCH(&*(jt+step/2));
      if (int res=tdeg_t_greater(j->u,u,order)){
	jt=j;
	if (res==2)
	  return true;
      }
      else
	jtend=j;
    }
  }

  template<class tdeg_t,class modint_t>
  void makelinesplit(const polymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<shifttype> & v){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=R.coord.begin(),jtend=R.coord.end();
    unsigned pos=0;
    double nop1=double(R.coord.size()); 
    double nop2=4*p.coord.size()*std::log(nop1)/std::log(2.0);
    bool dodicho=nop2<nop1;
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	/* new faster code */
	if (dodicho && dicho<tdeg_t,modint_t>(jt,jtend,u,R.order)){
	  pushsplit(v,pos,unsigned(jt-R.coord.begin()));
	  ++jt;
	  continue;
	}
	/* end new faster code */
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    pushsplit(v,pos,unsigned(jt-R.coord.begin()));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	/* new faster code */
	if (dodicho && dicho<tdeg_t,modint_t>(jt,jtend,u,R.order)){
	  pushsplit(v,pos,unsigned(jt-R.coord.begin()));
	  ++jt;
	  continue;
	}
	/* end new faster code */
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    pushsplit(v,pos,unsigned(jt-R.coord.begin()));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  // return true if all shifts are <=0xffff
  bool checkshortshifts(const vector<shifttype> & v){
    if (v.empty())
      return false;
    const shifttype * it=&v.front(),*itend=it+v.size();
    // ignore first, it's not a shift
    unsigned pos;
    next_index(pos,it);
    for (;it!=itend;++it){
      if (!*it)
	return false;
    }
    return true;
  }

  template<class tdeg_t,class modint_t>
  void makelinesplit(const poly8<tdeg_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<shifttype> & v){
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=R.coord.begin(),jt0=jt,jtend=R.coord.end();
    unsigned pos=0;
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    pushsplit(v,pos,unsigned(jt-jt0));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    pushsplit(v,pos,unsigned(jt-jt0));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void makelinesplitu(const polymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<unsigned> & vu){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=R.coord.begin(),jt0=jt,jtend=R.coord.end();
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    vu.push_back(hashgcd_U(jt-jt0));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    vu.push_back(hashgcd_U(jt-jt0));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void makelinesplits(const polymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<unsigned short> & vu){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end(),jt=R.coord.begin(),jt0=jt,jtend=R.coord.end();
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    vu.push_back(hashgcd_U(jt-jt0));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    vu.push_back(hashgcd_U(jt-jt0));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }


#define GIAC_Z

  // cache protection for rur ideal dim computation
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_t rur_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

  template<class tdeg_t,class modint_t,class modint_t2>
  void rref_f4buchbergermodsplit_interreduce(vectpolymod<tdeg_t,modint_t> & f4buchbergerv,const vector<unsigned> & f4buchbergervG,vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,unsigned excluded,const vectpolymod<tdeg_t,modint_t> & quo,const polymod<tdeg_t,modint_t> & R,modint_t env,vector<int> & permutation){
    // step2: for each monomials of quo[i], shift res[G[i]] by monomial
    // set coefficient in a line of a matrix M, columns are R monomials indices
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin build M" << '\n';
    unsigned N=unsigned(R.coord.size()),i,j=0;
    if (N==0) return;
#if GIAC_SHORTSHIFTTYPE==16
    bool useshort=true;
#else
    bool useshort=N<=0xffff;
#endif
    unsigned nrows=0;
    for (i=0;i<G.size();++i){
      nrows += unsigned(quo[i].coord.size());
    }
    unsigned c=N;
    double sknon0=0;
    vector<used_t> used(N,0);
    unsigned usedcount=0,zerolines=0;
    vector< vector<modint_t> > K(f4buchbergervG.size());
    vector<vector<unsigned short> > Mindex;
    vector<vector<unsigned> > Muindex;
    vector< vector<modint_t> > Mcoeff(G.size());
    vector<coeffindex_t> coeffindex;
    if (useshort)
      Mindex.reserve(nrows);
    else
      Muindex.reserve(nrows);
    coeffindex.reserve(nrows);
    vector<sparse_element> atrier;
    atrier.reserve(nrows);
    for (i=0;i<G.size();++i){
      Mcoeff[i].reserve(res[G[i]].coord.size());
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      if (useshort){
	for (;jt!=jtend;++j,++jt){
	  Mindex.push_back(vector<unsigned short>(0));
#if GIAC_SHORTSHIFTTYPE==16
	  Mindex[j].reserve(int(1.1*res[G[i]].coord.size()));
#else
	  Mindex[j].reserve(res[G[i]].coord.size());
#endif
	}
      }
      else {
	for (;jt!=jtend;++j,++jt){
	  Muindex.push_back(vector<unsigned>(0));
	  Muindex[j].reserve(res[G[i]].coord.size());
	}
      }
    }
    for (i=0,j=0;i<G.size();++i){
      // copy coeffs of res[G[i]] in Mcoeff
      copycoeff(res[G[i]],Mcoeff[i]);
      // for each monomial of quo[i], find indexes and put in Mindex
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      for (;jt!=jtend;++j,++jt){
	coeffindex.push_back(coeffindex_t(N<=0xffff,i));
	if (useshort){
#if GIAC_SHORTSHIFTTYPE==16
	  makelinesplit(res[G[i]],&jt->u,R,Mindex[j]);
	  if (!coeffindex.back().b)
	    coeffindex.back().b=checkshortshifts(Mindex[j]);
	  atrier.push_back(sparse_element(first_index(Mindex[j]),j));
#else
	  makelinesplits(res[G[i]],&jt->u,R,Mindex[j]);
	  atrier.push_back(sparse_element(Mindex[j].front(),j));
#endif
	}
	else {
	  makelinesplitu(res[G[i]],&jt->u,R,Muindex[j]);
	  atrier.push_back(sparse_element(Muindex[j].front(),j));
	}
      }
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " end build Mindex/Mcoeff rref_f4buchbergermodsplit_interreduce" << '\n';
    // should not sort but compare res[G[i]]*quo[i] monomials to build M already sorted
    // CERR << "before sort " << M << '\n';
    sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1); 
    vector<coeffindex_t> coeffindex1(atrier.size());
    double mem=0; // mem*4=number of bytes allocated for M1
    if (useshort){
      vector< vector<unsigned short> > Mindex1(atrier.size());
      for (i=0;i<atrier.size();++i){
	swap(Mindex1[i],Mindex[atrier[i].pos]);
	mem += Mindex1[i].size();
	swap(coeffindex1[i],coeffindex[atrier[i].pos]);
      }
      swap(Mindex,Mindex1);
      nrows=unsigned(Mindex.size());
    }
    else {
      vector< vector<unsigned> > Muindex1(atrier.size());
      for (i=0;i<atrier.size();++i){
	swap(Muindex1[i],Muindex[atrier[i].pos]);
	mem += Muindex1[i].size();
	swap(coeffindex1[i],coeffindex[atrier[i].pos]);
      }
      swap(Muindex,Muindex1);
      nrows=unsigned(Muindex.size());
    }
    swap(coeffindex,coeffindex1);
    vector<unsigned> firstpos(atrier.size());
    for (i=0;i < atrier.size();++i){
      firstpos[i]=atrier[i].val;
    }
    bool freemem=true; // mem>4e7; // should depend on real memory available
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " Mindex sorted, rows " << nrows << " columns " << N << " terms " << mem << " ratio " << (mem/nrows)/N <<'\n';
    // CERR << "after sort " << M << '\n';
    // step3 reduce
    vector<modint_t> v(N); 
    vector<modint_t2> v64(N);
#ifdef x86_64
    vector<int128_t> v128(N);
#endif
#ifdef GIAC_Z
    if (N<nrows){
      CERR << "Error " << N << "," << nrows << '\n';
      return;
    }
    unsigned Kcols=N-nrows;
    unsigned effectivef4buchbergervGsize=0;
    for (unsigned i=0;i<f4buchbergervG.size();++i){
      if (!f4buchbergerv[f4buchbergervG[i]].coord.empty())
	++effectivef4buchbergervGsize;
    }
    vector<modint_t> lescoeffs;
    lescoeffs.reserve(Kcols*effectivef4buchbergervGsize);
    // vector<modint_t> lescoeffs(Kcols*effectivef4buchbergervGsize);
    // vector<modint_t>::iterator coeffit=lescoeffs.begin();
    if (debug_infolevel>1)
      CERR << "Capacity for coeffs " << lescoeffs.size() << '\n';
    vector<unsigned> lebitmap(((N>>5)+1)*effectivef4buchbergervGsize);
    unsigned * bitmap=&lebitmap.front();
#else
    vector< vector<sparse_element> > SK(f4buchbergerv.size());
#endif
    for (i=0;i<f4buchbergervG.size();++i){
      if (!f4buchbergerv[f4buchbergervG[i]].coord.empty()){
	makeline<tdeg_t>(f4buchbergerv[f4buchbergervG[i]],0,R,v);
	//CERR << v << '\n';
#ifdef x86_64
	if (useshort){
	  if (env<(1<<24)){
#if GIAC_SHORTSHIFTTYPE==16
	    c=giacmin(c,reducef4buchbergersplit(v,Mindex,firstpos,Mcoeff,coeffindex,env,v64));
#else
	    c=giacmin(c,reducef4buchbergersplits(v,Mindex,Mcoeff,coeffindex,env,v64));
#endif
	  }
	  else {
#if GIAC_SHORTSHIFTTYPE==16
	    c=giacmin(c,reducef4buchbergersplit128(v,Mindex,firstpos,Mcoeff,coeffindex,env,v128));
#else
	    c=giacmin(c,reducef4buchbergersplit128s(v,Mindex,Mcoeff,coeffindex,env,v128));
#endif
	  }
	}
	else {
	  if (env<(1<<24))
	    c=giacmin(c,reducef4buchbergersplitu(v,Muindex,Mcoeff,coeffindex,env,v64));
	  else
	    c=giacmin(c,reducef4buchbergersplit128u(v,Muindex,Mcoeff,coeffindex,env,v128));
	}
#else
	if (useshort){
#if GIAC_SHORTSHIFTTYPE==16
	  c=giacmin(c,reducef4buchbergersplit(v,Mindex,firstpos,Mcoeff,coeffindex,env,v64));
#else
	  c=giacmin(c,reducef4buchbergersplits(v,Mindex,Mcoeff,coeffindex,env,v64));
#endif
	}
	else 
	  c=giacmin(c,reducef4buchbergersplitu(v,Muindex,Mcoeff,coeffindex,env,v64));
#endif
	// convert v to a sparse vector in SK and update used
	if (freemem){ 
	  polymod<tdeg_t,modint_t> clearer; swap(f4buchbergerv[f4buchbergervG[i]].coord,clearer.coord); 
	}
#ifdef GIAC_Z
	// zconvert(v,coeffit,bitmap,used); bitmap += (N>>5)+1;
	zconvert_(v,lescoeffs,bitmap,used); bitmap += (N>>5)+1;
#else
	convert(v,SK[i],used);
#endif
	//CERR << v << '\n' << SK[i] << '\n';
      }
    }
#if 0 // def GIAC_Z
    if (debug_infolevel>1) CERR << "Total size for coeffs " << coeffit-lescoeffs.begin() << '\n';
    if (freemem){ 
      for (i=0;i<f4buchbergervG.size();++i){
	polymod<tdeg_t,modint_t> clearer; swap(f4buchbergerv[f4buchbergervG[i]].coord,clearer.coord); 
      }
    }
#endif
    Mindex.clear(); Muindex.clear();
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv split reduced " << f4buchbergervG.size() << " polynoms over " << N << " monomials, start at " << c << '\n';
    for (i=0;i<N;++i)
      usedcount += (used[i]>0);
    if (debug_infolevel>1){
      CERR << CLOCK()*1e-6 << " number of non-zero columns " << usedcount << " over " << N << '\n'; // usedcount should be approx N-M.size()=number of cols of M-number of rows
      if (debug_infolevel>3)
	CERR << " column split used " << used << '\n';
    }
    // create dense matrix K 
#ifdef GIAC_Z
    bitmap=&lebitmap.front();
    create_matrix(lescoeffs,bitmap,(N>>5)+1,used,K);
    if (freemem){ 
      // clear memory required for lescoeffs
      vector<modint_t> tmp; lescoeffs.swap(tmp); 
      vector<unsigned> tmp1; lebitmap.swap(tmp1);
    }
#else
    for (i=0; i<K.size(); ++i){
      vector<modint_t> & v =K[i];
      if (SK[i].empty()){
	++zerolines;
	continue;
      }
      sknon0 += SK[i].size();
      v.resize(usedcount);
      vector<modint_t>::iterator vt=v.begin();
      vector<used_t>::const_iterator ut=used.begin(),ut0=ut;
      vector<sparse_element>::const_iterator st=SK[i].begin(),stend=SK[i].end();
      for (j=0;st!=stend;++j,++ut){
	if (!*ut) 
	  continue;
	if (j==st->pos){
	  *vt=st->val;
	  ++st;
	}
	++vt;
      }
#if 1
      vector<sparse_element> clearer;
      swap(SK[i],clearer); // clear SK[i] memory
#endif
      // CERR << used << '\n' << SK[i] << '\n' << K[i] << '\n';
    } // end create dense matrix K
#endif // GIAC_Z
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " rref " << K.size() << "x" << usedcount << " non0 " << sknon0 << " ratio " << (sknon0/K.size())/usedcount << " nulllines " << zerolines << '\n';
    vecteur pivots; vector<int> maxrankcols; longlong idet;
    //CERR << K << '\n';
    smallmodrref(1,K,pivots,permutation,maxrankcols,idet,0,int(K.size()),0,usedcount,1/* fullreduction*/,0/*dontswapbelow*/,env,0/* rrefordetorlu*/,true,0,true,-1);
    //CERR << K << "," << permutation << '\n';
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=R.coord.begin(),itend=R.coord.end();
    vector<int> permu=perminv(permutation);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv interreduced" << '\n';
    for (i=0;i<f4buchbergervG.size();++i){
      // CERR << v << '\n';
      vector< T_unsigned<modint_t,tdeg_t> > & Pcoord=f4buchbergerv[f4buchbergervG[i]].coord;
      Pcoord.clear();
      vector<modint_t> & v =K[permu[i]];
      if (v.empty())
	continue;
      unsigned vcount=0;
      typename vector<modint_t>::const_iterator vt=v.begin(),vtend=v.end();
      for (;vt!=vtend;++vt){
	if (*vt)
	  ++vcount;
      }
      Pcoord.reserve(vcount);
      vector<used_t>::const_iterator ut=used.begin();
      for (vt=v.begin(),it=R.coord.begin();it!=itend;++ut,++it){
	if (!*ut)
	  continue;
	modint_t coeff=*vt;
	++vt;
	if (coeff!=0)
	  Pcoord.push_back(T_unsigned<modint_t,tdeg_t>(coeff,it->u));
      }
      if (!Pcoord.empty() && Pcoord.front().g!=1){
	smallmultmod(invmod(Pcoord.front().g,env),f4buchbergerv[f4buchbergervG[i]],env);	
	Pcoord.front().g=1;
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void rref_f4buchbergermod_nointerreduce(vectpolymod<tdeg_t,modint_t> & f4buchbergerv,const vector<unsigned> & f4buchbergervG,vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,unsigned excluded,const vectpolymod<tdeg_t,modint_t> & quo,const polymod<tdeg_t,modint_t> & R,modint_t env,vector<int> & permutation){
    unsigned N=unsigned(R.coord.size()),i,j=0;
    for (i=0;i<G.size();++i){
      if (!quo[i].coord.empty())
	break;
    }
    if (i==G.size()){
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " No inter-reduction" << '\n';
      return;
    }
    // step2: for each monomials of quo[i], shift res[G[i]] by monomial
    // set coefficient in a line of a matrix M, columns are R monomials indices
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin build M" << '\n';
    vector< vector<sparse_element> > M;
    M.reserve(N);
    vector<sparse_element> atrier;
    atrier.reserve(N);
    for (i=0;i<G.size();++i){
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      for (;jt!=jtend;++j,++jt){
	M.push_back(vector<sparse_element>(0));
	M[j].reserve(res[G[i]].coord.size());
	makeline(res[G[i]],&jt->u,R,M[j]);
	atrier.push_back(sparse_element(M[j].front().pos,j));
      }
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " end build M" << '\n';
    // should not sort but compare res[G[i]]*quo[i] monomials to build M already sorted
    // CERR << "before sort " << M << '\n';
    sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1); 
    vector< vector<sparse_element> > M1(atrier.size());
    for (i=0;i<atrier.size();++i){
      swap(M1[i],M[atrier[i].pos]);
    }
    swap(M,M1);
    // sort(M.begin(),M.end(),tri);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " M sorted, rows " << M.size() << " columns " << N << " #basis to reduce" << f4buchbergervG.size() << '\n';
    // CERR << "after sort " << M << '\n';
    // step3 reduce
    unsigned c=N;
    vector<modint_t> v(N);
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=R.coord.begin(),itend=R.coord.end();
#ifdef x86_64
    vector<int128_t> v128(N);
#endif
    for (i=0;i<f4buchbergervG.size();++i){
      if (!f4buchbergerv[f4buchbergervG[i]].coord.empty()){
	makeline<tdeg_t>(f4buchbergerv[f4buchbergervG[i]],0,R,v);
	// CERR << v << '\n';
#ifdef x86_64
	/* if (N>=4096)
	  c=giacmin(c,reducef4buchberger(v,M,env));
	  else */
	  c=giacmin(c,reducef4buchberger_64(v,M,env,v128));
#else
	c=giacmin(c,reducef4buchberger(v,M,env));
#endif
	vector< T_unsigned<modint_t,tdeg_t> > & Pcoord=f4buchbergerv[f4buchbergervG[i]].coord;
	Pcoord.clear();
	unsigned vcount=0;
	typename vector<modint_t>::const_iterator vt=v.begin(),vtend=v.end();
	for (;vt!=vtend;++vt){
	  if (*vt)
	    ++vcount;
	}
	Pcoord.reserve(vcount);
	for (vt=v.begin(),it=R.coord.begin();it!=itend;++it){
	  modint_t coeff=*vt;
	  ++vt;
	  if (coeff!=0)
	    Pcoord.push_back(T_unsigned<modint_t,tdeg_t>(coeff,it->u));
	}
	if (!Pcoord.empty() && Pcoord.front().g!=1){
	  smallmultmod(invmod(Pcoord.front().g,env),f4buchbergerv[f4buchbergervG[i]],env);	
	  Pcoord.front().g=1;
	}
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void rref_f4buchbergermod(vectpolymod<tdeg_t,modint_t> & f4buchbergerv,vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,unsigned excluded,const vectpolymod<tdeg_t,modint_t> & quo,const polymod<tdeg_t,modint_t> & R,modint_t env,vector<int> & permutation,bool split){
    vector<unsigned> f4buchbergervG(f4buchbergerv.size());
    for (unsigned i=0;i<f4buchbergerv.size();++i)
      f4buchbergervG[i]=i;
#if 0
    rref_f4buchbergermod(f4buchbergerv,f4buchbergervG,res,G,excluded,quo,R,env,permutation,true);
#else
    if (//1
	split
	//0
	)
      rref_f4buchbergermodsplit_interreduce<tdeg_t,modint_t,modint2>(f4buchbergerv,f4buchbergervG,res,G,excluded,quo,R,env,permutation);
    else
      rref_f4buchbergermod_interreduce<tdeg_t,modint_t,modint2>(f4buchbergerv,f4buchbergervG,res,G,excluded,quo,R,env,permutation);
#endif
  }

  template<class tdeg_t,class modint_t>
  struct info_t {
    vectpolymod<tdeg_t,modint_t> quo,quo2;
    polymod<tdeg_t,modint_t> R,R2;
    vector<int> permu;
    vector< paire > B;
    vector<unsigned> G;
    unsigned nonzero;
  };

  template<class tdeg_t,class modint_t>
  void reducemodf4buchberger(vectpolymod<tdeg_t,modint_t> & f4buchbergerv,vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,unsigned excluded, modint_t env,info_t<tdeg_t,modint_t> & info_tmp){
    polymod<tdeg_t,modint_t> allf4buchberger(f4buchbergerv.front().order,f4buchbergerv.front().dim),rem(f4buchbergerv.front().order,f4buchbergerv.front().dim);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchberger begin collect monomials on #polys " << f4buchbergerv.size() << '\n';
    // collect all terms in f4buchbergerv
    collect(f4buchbergerv,allf4buchberger);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchberger symbolic preprocess" << '\n';
    // find all monomials required to reduce all polymod<tdeg_t,modint_t> in f4buchberger with res[G[.]]
    symbolic_preprocess(allf4buchberger,res,G,excluded,info_tmp.quo,rem,&info_tmp.R);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchberger end symbolic preprocess" << '\n';
    // build a matrix with first lines res[G[.]]*quo[.] in terms of monomials in S
    // and finishing with lines of f4buchbergerv
    // rref (below) the matrix and find the last lines in f4buchbergerv
    rref_f4buchbergermod(f4buchbergerv,res,G,excluded,info_tmp.quo,info_tmp.R,env,info_tmp.permu,true); // do splitting
  }

  // v -= v1, assumes that no overflow can happen (modulo must be 2^30)
  void sub(vector<modint> & v,const vector<modint> & v1,modint env){
    vector<modint>::const_iterator jt=v1.begin();
    vector<modint>::iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++jt,++it){
      *it -= *jt;
      if (*it>-env && *it<env)
	continue;
      if (*it>env)
	*it -=env;
      else
	*it += env;
    }
#if 0
    // normalize first element to 1
    for (it=v.begin();it!=itend;++it){
      if (*it)
	break;
    }
    if (it!=itend){
      modint c=invmod(*it,env);
      *it=1;
      for (++it;it!=itend;++it){
	if (*it)
	  *it=(extend(c)*(*it))%env;
      }
    }
#endif
  }

  void sub(vector<modint2> & v,const vector<modint2> & v1){
    vector<modint2>::const_iterator jt=v1.begin();
    vector<modint2>::iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++jt,++it){
      *it -= *jt;
    }
  }

  void sub(vector<double> & v,const vector<modint2> & v1){
    vector<modint2>::const_iterator jt=v1.begin();
    vector<double>::iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++jt,++it){
      *it -= *jt;
    }
  }

  template<class tdeg_t,class modint_t,class modint_t2>
  int f4mod(vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,modint_t env,vector< paire > & B,vectpolymod<tdeg_t,modint_t> & f4buchbergerv,bool learning,unsigned & learned_position,vector< paire > * pairs_reducing_to_zero,vector< info_t<tdeg_t,modint_t> >* f4buchberger_info,unsigned & f4buchberger_info_position,bool recomputeR){
    if (B.empty())
      return 0;
    vector<tdeg_t> leftshift(B.size());
    vector<tdeg_t> rightshift(B.size());
    leftright(res,B,leftshift,rightshift);
    f4buchbergerv.resize(B.size());
    info_t<tdeg_t,modint_t> info_tmp;
    unsigned nonzero=unsigned(B.size());
    info_t<tdeg_t,modint_t> * info_ptr=&info_tmp;
    if (!learning && f4buchberger_info && f4buchberger_info_position<f4buchberger_info->size()){
      info_ptr=&(*f4buchberger_info)[f4buchberger_info_position];
      ++f4buchberger_info_position;
      nonzero=info_ptr->nonzero;
    }
    else {
      polymod<tdeg_t,modint_t> all(res[B[0].first].order,res[B[0].first].dim),rem;
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " f4buchberger begin collect monomials on #polys " << f4buchbergerv.size() << '\n';
      collect(res,B,all,leftshift,rightshift);
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " f4buchberger symbolic preprocess" << '\n';
      symbolic_preprocess(all,res,G,-1,info_tmp.quo,rem,&info_tmp.R);
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " end symbolic preprocess, rem size " << rem.coord.size() << '\n';
    }
    polymod<tdeg_t,modint_t> & R = info_ptr->R;
    vectpolymod<tdeg_t,modint_t> & quo = info_ptr->quo;
	unsigned N = unsigned(R.coord.size()), i, j = 0;
    if (N==0){
      if (learning && f4buchberger_info)
	f4buchberger_info->push_back(*info_ptr);
      return 1;
    }
#if GIAC_SHORTSHIFTTYPE==16
    bool useshort=true;
#else
    bool useshort=N<=0xffff;
#endif
    unsigned nrows=0;
    for (i=0;i<G.size();++i){
		nrows += unsigned(quo[i].coord.size());
    }
    unsigned c=N;
    double sknon0=0;
    vector<used_t> used(N,0);
    unsigned usedcount=0,zerolines=0;
    vector< vector<modint_t> > K(B.size());
    vector<vector<unsigned short> > Mindex;
    vector<vector<unsigned> > Muindex;
    vector< vector<modint_t> > Mcoeff(G.size());
    vector<coeffindex_t> coeffindex;
    if (useshort)
      Mindex.reserve(nrows);
    else
      Muindex.reserve(nrows);
    coeffindex.reserve(nrows);
    vector<sparse_element> atrier;
    atrier.reserve(nrows);
    for (i=0;i<G.size();++i){
      Mcoeff[i].reserve(res[G[i]].coord.size());
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      if (useshort){
	for (;jt!=jtend;++j,++jt){
	  Mindex.push_back(vector<unsigned short>(0));
#if GIAC_SHORTSHIFTTYPE==16
	  Mindex[j].reserve(int(1.1*res[G[i]].coord.size()));
#else
	  Mindex[j].reserve(res[G[i]].coord.size());
#endif
	}
      }
      else {
	for (;jt!=jtend;++j,++jt){
	  Muindex.push_back(vector<unsigned>(0));
	  Muindex[j].reserve(res[G[i]].coord.size());
	}
      }
    }
    for (i=0,j=0;i<G.size();++i){
      // copy coeffs of res[G[i]] in Mcoeff
      copycoeff(res[G[i]],Mcoeff[i]);
      // for each monomial of quo[i], find indexes and put in Mindex
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      for (;jt!=jtend;++j,++jt){
	coeffindex.push_back(coeffindex_t(N<=0xffff,i));
	if (useshort){
#if GIAC_SHORTSHIFTTYPE==16
	  makelinesplit(res[G[i]],&jt->u,R,Mindex[j]);
	  if (!coeffindex.back().b)
	    coeffindex.back().b=checkshortshifts(Mindex[j]);
	  atrier.push_back(sparse_element(first_index(Mindex[j]),j));
#else
	  makelinesplits(res[G[i]],&jt->u,R,Mindex[j]);
	  atrier.push_back(sparse_element(Mindex[j].front(),j));
#endif
	}
	else {
	  makelinesplitu(res[G[i]],&jt->u,R,Muindex[j]);
	  atrier.push_back(sparse_element(Muindex[j].front(),j));
	}
      }
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " end build Mindex/Mcoeff f4mod" << '\n';
    // should not sort but compare res[G[i]]*quo[i] monomials to build M already sorted
    // CERR << "before sort " << M << '\n';
    sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1); 
    vector<coeffindex_t> coeffindex1(atrier.size());
    double mem=0; // mem*4=number of bytes allocated for M1
    if (useshort){
      vector< vector<unsigned short> > Mindex1(atrier.size());
      for (i=0;i<atrier.size();++i){
	swap(Mindex1[i],Mindex[atrier[i].pos]);
	mem += Mindex1[i].size();
	swap(coeffindex1[i],coeffindex[atrier[i].pos]);
      }
      swap(Mindex,Mindex1);
	  nrows = unsigned(Mindex.size());
    }
    else {
      vector< vector<unsigned> > Muindex1(atrier.size());
      for (i=0;i<atrier.size();++i){
	swap(Muindex1[i],Muindex[atrier[i].pos]);
	mem += Muindex1[i].size();
	swap(coeffindex1[i],coeffindex[atrier[i].pos]);
      }
      swap(Muindex,Muindex1);
	  nrows = unsigned(Muindex.size());
    }
    swap(coeffindex,coeffindex1);
    vector<unsigned> firstpos(atrier.size());
    for (i=0;i < atrier.size();++i){
      firstpos[i]=atrier[i].val;
    }
    bool freemem=mem>4e7; // should depend on real memory available
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " Mindex sorted, rows " << nrows << " columns " << N << " terms " << mem << " ratio " << (mem/nrows)/N <<'\n';
    // CERR << "after sort " << M << '\n';
    // step3 reduce
    vector<modint_t> v(N);
    vector<modint_t2> v64(N);
#ifdef x86_64
    vector<int128_t> v128(N);
#endif
    if (N<nrows){
      CERR << "Error " << N << "," << nrows << '\n';
      return -1;
    }
    unsigned Kcols=N-nrows;
    vector<unsigned> lebitmap(((N>>5)+1)*B.size());
    unsigned * bitmap=&lebitmap.front();
    for (i=0;i<B.size();++i){
      paire bk=B[i];
      if (!learning && pairs_reducing_to_zero && learned_position<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[learned_position]){
	if (debug_infolevel>2)
	  CERR << bk << " f4buchberger learned " << learned_position << '\n';
	++learned_position;
	unsigned tofill=(N>>5)+1;
	fill(bitmap,bitmap+tofill,0);
	bitmap += tofill;
	continue;
      }
      makeline(res[bk.first],&leftshift[i],R,v,1);
      makelinesub(res[bk.second],&rightshift[i],R,v,1,env);
      // CERR << v << '\n' << v2 << '\n';
      // sub(v,v2,env);
      // CERR << v << '\n';
#ifdef x86_64
      if (useshort){
	if (env<(1<<24)){
#if GIAC_SHORTSHIFTTYPE==16
	  c=giacmin(c,reducef4buchbergersplit(v,Mindex,firstpos,Mcoeff,coeffindex,env,v64));
#else
	  c=giacmin(c,reducef4buchbergersplits(v,Mindex,Mcoeff,coeffindex,env,v64));
#endif
	}
	else {
#if GIAC_SHORTSHIFTTYPE==16
	  c=giacmin(c,reducef4buchbergersplit128(v,Mindex,firstpos,Mcoeff,coeffindex,env,v128));
#else
	  c=giacmin(c,reducef4buchbergersplit128s(v,Mindex,Mcoeff,coeffindex,env,v128));
#endif
	}
      }
      else {
	if (env<(1<<24))
	  c=giacmin(c,reducef4buchbergersplitu(v,Muindex,Mcoeff,coeffindex,env,v64));
	else
	  c=giacmin(c,reducef4buchbergersplit128u(v,Muindex,Mcoeff,coeffindex,env,v128));
      }
#else // x86_64
      if (useshort){
#if GIAC_SHORTSHIFTTYPE==16
	c=giacmin(c,reducef4buchbergersplit(v,Mindex,firstpos,Mcoeff,coeffindex,env,v64));
#else
	c=giacmin(c,reducef4buchbergersplits(v,Mindex,Mcoeff,coeffindex,env,v64));
#endif
      }
      else 
	c=giacmin(c,reducef4buchbergersplitu(v,Muindex,Mcoeff,coeffindex,env,v64));
#endif // x86_64
      // zconvert(v,coeffit,bitmap,used); bitmap += (N>>5)+1;
      K[i].reserve(Kcols);
      zconvert_(v,K[i],bitmap,used); bitmap += (N>>5)+1;
      //CERR << v << '\n' << SK[i] << '\n';
    } // end for (i=0;i<B.size();++i)
    Mindex.clear(); Muindex.clear();
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv split reduced " << B.size() << " polynoms over " << N << " monomials, start at " << c << '\n';
    for (i=0;i<N;++i)
      usedcount += (used[i]>0);
    if (debug_infolevel>1){
      CERR << CLOCK()*1e-6 << " number of non-zero columns " << usedcount << " over " << N << '\n'; // usedcount should be approx N-M.size()=number of cols of M-number of rows
      if (debug_infolevel>3)
	CERR << " column split used " << used << '\n';
    }
    // create dense matrix K 
    bitmap=&lebitmap.front();
    unsigned zeros=create_matrix(bitmap,(N>>5)+1,used,K);
    // clear memory required for lescoeffs
    //vector<modint_t> tmp; lescoeffs.swap(tmp); 
    { vector<unsigned> tmp1; lebitmap.swap(tmp1); }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " dense_rref " << K.size()-zeros << "(" << K.size() << ")" << "x" << usedcount << " ncoeffs=" << double(K.size()-zeros)*usedcount*1e-6 << "*1e6\n";
    vecteur pivots; vector<int> permutation,maxrankcols; longlong idet;
    // CERR << K << '\n';
    smallmodrref(1,K,pivots,permutation,maxrankcols,idet,0,int(K.size()),0,usedcount,1/* fullreduction*/,0/*dontswapbelow*/,env,0/* rrefordetorlu*/,true,0,true,-1);
    //CERR << K << '\n';
    unsigned first0 = unsigned(pivots.size());
    if (first0<K.size() && (learning || !f4buchberger_info)){
      vector<modint_t> & tmpv=K[first0];
      for (i=0;i<tmpv.size();++i){
	if (tmpv[i])
	  break;
      }
      if (i==tmpv.size()){
	unsigned Ksize = unsigned(K.size());
	K.resize(first0);
	K.resize(Ksize);
      }
    }
    //CERR << permutation << K << '\n';
    if (!learning && f4buchberger_info){
      // check that permutation is the same as learned permutation
      for (unsigned j=0;j<permutation.size();++j){
	if (permutation[j]!=info_ptr->permu[j]){
	  CERR << "learning failed"<<'\n';
	  return -1;
	}
      }
    }
    if (learning)
      info_ptr->permu=permutation;
    // CERR << K << "," << permutation << '\n';
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=R.coord.begin(),itend=R.coord.end();
    // vector<int> permu=perminv(permutation);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv interreduced" << '\n';
    for (i=0;i<f4buchbergerv.size();++i){
      // CERR << v << '\n';
      vector< T_unsigned<modint_t,tdeg_t> > & Pcoord=f4buchbergerv[permutation[i]].coord;
      Pcoord.clear();
      vector<modint_t> & v =K[i];
      if (v.empty()){
	continue;
      }
      unsigned vcount=0;
      typename vector<modint_t>::const_iterator vt=v.begin(),vtend=v.end();
      for (;vt!=vtend;++vt){
	if (*vt)
	  ++vcount;
      }
      Pcoord.reserve(vcount);
      vector<used_t>::const_iterator ut=used.begin();
      for (vt=v.begin(),it=R.coord.begin();it!=itend;++ut,++it){
	if (!*ut)
	  continue;
	modint_t coeff=*vt;
	++vt;
	if (coeff!=0)
	  Pcoord.push_back(T_unsigned<modint_t,tdeg_t>(coeff,it->u));
      }
      if (!Pcoord.empty() && Pcoord.front().g!=1){
	smallmultmod(invmod(Pcoord.front().g,env),f4buchbergerv[permutation[i]],env);	
	Pcoord.front().g=1;
      }
      if (freemem){
	vector<modint_t> tmp; tmp.swap(v);
      }
    }
    if (learning && f4buchberger_info){
#if 0
      f4buchberger_info->push_back(*info_ptr);
#else
      info_t<tdeg_t,modint_t> tmp;
      f4buchberger_info->push_back(tmp);
      info_t<tdeg_t,modint_t> & i=f4buchberger_info->back();
      swap(i.quo,info_ptr->quo);
      swap(i.permu,info_ptr->permu);
      swap(i.R.coord,info_ptr->R.coord);
      i.R.order=info_ptr->R.order;
      i.R.dim=info_ptr->R.dim;
#endif
    }
    return 1;
  }

  template<class tdeg_t,class modint_t>
  bool apply(vector<int> permu,vectpolymod<tdeg_t,modint_t> & res){
    vectpolymod<tdeg_t,modint_t> tmp;
    for (unsigned i=0;i<res.size();++i){
      tmp.push_back(polymod<tdeg_t,modint_t>(res.front().order,res.front().dim));
      swap(tmp[i].coord,res[permu[i]].coord);
      tmp[i].sugar=res[permu[i]].sugar;
    }
    swap(tmp,res);
    return true;
  }

  template<class tdeg_t,class modint_t>
  int f4mod(vectpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,modint_t env,vector< paire > & smallposp,vectpolymod<tdeg_t,modint_t> & f4buchbergerv,bool learning,unsigned & learned_position,vector< paire > * pairs_reducing_to_zero,info_t<tdeg_t,modint_t> & information,vector< info_t<tdeg_t,modint_t> >* f4buchberger_info,unsigned & f4buchberger_info_position,bool recomputeR, polymod<tdeg_t,modint_t> & TMP1,polymod<tdeg_t,modint_t> & TMP2){
    // Improve: we don't really need to compute the s-polys here
    // it's sufficient to do that at linalg step
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " Computing s-polys " << smallposp.size() << '\n';
    if (f4buchbergerv.size()<smallposp.size())
      f4buchbergerv.clear();
    f4buchbergerv.resize(smallposp.size());
    for (unsigned i=0;i<smallposp.size();++i){
      f4buchbergerv[i].dim=TMP1.dim; f4buchbergerv[i].order=TMP1.order;
      paire bk=smallposp[i];
      if (!learning && pairs_reducing_to_zero && f4buchberger_info && learned_position<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[learned_position]){
	if (debug_infolevel>2)
	  CERR << bk << " f4buchberger learned " << learned_position << '\n';
	++learned_position;
	f4buchbergerv[i].coord.clear();
	continue;
      }
      if (debug_infolevel>2)
	CERR << bk << " f4buchberger not learned " << learned_position << '\n';
      if (debug_infolevel>2 && (equalposcomp(G,bk.first)==0 || equalposcomp(G,bk.second)==0))
	CERR << CLOCK()*1e-6 << " mod reducing pair with 1 element not in basis " << bk << '\n';
      // polymod<tdeg_t,modint_t> h(res.front().order,res.front().dim);
      spolymod<tdeg_t,modint_t>(res[bk.first],res[bk.second],TMP1,TMP2,env);
      f4buchbergerv[i].coord.swap(TMP1.coord);
    }
    if (f4buchbergerv.empty())
      return 0;
    // reduce spolys in f4buchbergerv
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " base size " << G.size() << " reduce f4buchberger begin on " << f4buchbergerv.size() << " pairs" << '\n';
    if (!learning && f4buchberger_info && f4buchberger_info_position<f4buchberger_info->size()){
      info_t<tdeg_t,modint_t> & info=(*f4buchberger_info)[f4buchberger_info_position];
      // apply(perminv(info.permu),f4buchbergerv);
      if (recomputeR){
	swap(information.permu,info.permu);
	reducemodf4buchberger(f4buchbergerv,res,G,-1,env,info);
	swap(information.permu,info.permu);
      }
      else
	rref_f4buchbergermod(f4buchbergerv,res,G,-1,info.quo,info.R,env,information.permu,false); // don't split
      // apply(info.permu,f4buchbergerv);
      // information.permu should be identity, otherwise the whole learning process failed
      for (unsigned j=0;j<information.permu.size();++j){
	if (information.permu[j]!=info.permu[j]){
	  CERR << "learning failed"<<'\n';
	  return -1;
	}
      }
      ++f4buchberger_info_position;
    }
    else {
      reducemodf4buchberger(f4buchbergerv,res,G,-1,env,information);
      if (learning && f4buchberger_info){
#if 0
	f4buchberger_info->push_back(information);
#else
	info_t<tdeg_t,modint_t> tmp;
	f4buchberger_info->push_back(tmp);
	info_t<tdeg_t,modint_t> & i=f4buchberger_info->back();
	swap(i.quo,information.quo);
	swap(i.permu,information.permu);
	swap(i.R.coord,information.R.coord);
	i.R.order=information.R.order;
	i.R.dim=information.R.dim;
#endif
      }
    }
    return 1;
  }
  
  template<class tdeg_t,class modint_t>
  bool in_gbasisf4buchbergermod(vectpolymod<tdeg_t,modint_t> &res,unsigned ressize,vector<unsigned> & G,modint_t env,bool totdeg,vector< paire > * pairs_reducing_to_zero,vector< info_t<tdeg_t,modint_t> > * f4buchberger_info,bool recomputeR){
    unsigned cleared=0;
    unsigned learned_position=0,f4buchberger_info_position=0;
    bool sugar=false,learning=pairs_reducing_to_zero && pairs_reducing_to_zero->empty();
    if (debug_infolevel>1000)
      res.dbgprint(); // instantiate dbgprint()
    int capa=512;
    if (f4buchberger_info)
      capa=int(f4buchberger_info->capacity());
    polymod<tdeg_t,modint_t> TMP1(res.front().order,res.front().dim),TMP2(res.front().order,res.front().dim);
    vector< paire > B,BB;
    B.reserve(256); BB.reserve(256);
    vector<unsigned> smallposv;
    smallposv.reserve(256);
    info_t<tdeg_t,modint_t> information;
    order_t order=res.front().order;
    if (order.o==_PLEX_ORDER) // if (order!=_REVLEX_ORDER && order!=_TDEG_ORDER)
      totdeg=false;
    vector<unsigned> oldG(G);
    for (unsigned l=0;l<ressize;++l){
#ifdef GIAC_REDUCEMODULO
      reducesmallmod(res[l],res,G,-1,env,TMP2,!is_zero(env));
#endif      
      gbasis_updatemod(G,B,res,l,TMP2,env,true,oldG);
    }
    for (int age=1;!B.empty() && !interrupted && !ctrl_c;++age){
      if (age+1>=capa)
	return false; // otherwise reallocation will make pointers invalid
      oldG=G;
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " begin new iteration mod, " << env << " number of pairs: " << B.size() << ", base size: " << G.size() << '\n';
      if (1){
	// mem clear: remove res[i] if i is not in G nor in B
	vector<bool> clean(G.back()+1,true);
	for (unsigned i=0;i<G.size();++i){
	  clean[G[i]]=false;
	}
	for (unsigned i=0;i<B.size();++i){
	  clean[B[i].first]=false;
	  clean[B[i].second]=false;
	}
	for (unsigned i=0;i<clean.size() && i<res.size();++i){
	  if (clean[i] && res[i].coord.capacity()>1 && !res[i].coord.empty()){
	    cleared += unsigned(res[i].coord.capacity()) - 1;
	    polymod<tdeg_t,modint_t> clearer;
	    clearer.coord.push_back(res[i].coord.front());
	    clearer.coord.swap(res[i].coord);
	  }
	}
      }
      // find smallest lcm pair in B
      tdeg_t small0,cur;
      unsigned smallpos,smalltotdeg=0,curtotdeg=0,smallsugar=0,cursugar=0;
      smallposv.clear();
      for (smallpos=0;smallpos<B.size();++smallpos){
	if (!res[B[smallpos].first].coord.empty() && !res[B[smallpos].second].coord.empty())
	  break;
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return false;
      }
      index_lcm(res[B[smallpos].first].coord.front().u,res[B[smallpos].second].coord.front().u,small0,order);
      smallsugar=res[B[smallpos].first].sugar+(small0-res[B[smallpos].first].coord.front().u).total_degree(order);
      smalltotdeg=small0.total_degree(order);
      smallposv.push_back(smallpos);
      for (unsigned i=smallpos+1;i<B.size();++i){
#ifdef TIMEOUT
	control_c();
#endif
	if (interrupted || ctrl_c)
	  return false;
	if (res[B[i].first].coord.empty() || res[B[i].second].coord.empty())
	  continue;
	bool doswap=false;
	index_lcm(res[B[i].first].coord.front().u,res[B[i].second].coord.front().u,cur,order);
	cursugar=res[B[smallpos].first].sugar+(cur-res[B[smallpos].first].coord.front().u).total_degree(order);
	curtotdeg=cur.total_degree(order);
	if ( !totdeg || order.o==_PLEX_ORDER)
	  doswap=tdeg_t_strictly_greater(small0,cur,order);
	else {
	  if (sugar){
	    if (smallsugar!=cursugar)
	      doswap = smallsugar > cursugar;
	  }
	  else {
	    if (smalltotdeg!=curtotdeg)
	      doswap = smalltotdeg > curtotdeg;	      
	  }
	}
	if (doswap){
	  smallsugar=cursugar;
	  smalltotdeg=curtotdeg;
	  // CERR << "swap mod " << curtotdeg << " " << res[B[i].first].coord.front().u << " " << res[B[i].second].coord.front().u << '\n';
	  swap(small0,cur); // small=cur;
	  smallpos=i;
	  smallposv.clear();
	  smallposv.push_back(i);
	}
	else {
	  if (totdeg && curtotdeg==smalltotdeg && (!sugar || cursugar==smallsugar))
	    smallposv.push_back(i);
	}
      }
      if (smallposv.size()<=GBASISF4_BUCHBERGER){
	unsigned i=smallposv[0];
	paire bk=B[i];
	B.erase(B.begin()+i);
	if (!learning && pairs_reducing_to_zero && learned_position<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[learned_position]){
	  if (debug_infolevel>2)
	    CERR << bk << " learned " << learned_position << '\n';
	  ++learned_position;
	  continue;
	}
	if (debug_infolevel>2)
	  CERR << bk << " not learned " << learned_position << '\n';
	if (debug_infolevel>2 && (equalposcomp(G,bk.first)==0 || equalposcomp(G,bk.second)==0))
	  CERR << CLOCK()*1e-6 << " mod reducing pair with 1 element not in basis " << bk << '\n';
	// polymod<tdeg_t,modint_t> h(res.front().order,res.front().dim);
	spolymod<tdeg_t,modint_t>(res[bk.first],res[bk.second],TMP1,TMP2,env);
	if (debug_infolevel>1){
	  CERR << CLOCK()*1e-6 << " mod reduce begin, pair " << bk << " spoly size " << TMP1.coord.size() << " sugar degree " << TMP1.sugar << " totdeg deg " << TMP1.coord.front().u.total_degree(order) << " degree " << TMP1.coord.front().u << '\n';
	}
#if 0 // def GBASIS_HEAP
	heap_reducemod(TMP1,res,G,-1,information.quo,TMP2,env);
	swap(TMP1.coord,TMP2.coord);
#else
	reducemod(TMP1,res,G,-1,TMP1,env);
#endif
	if (debug_infolevel>1){
	  if (debug_infolevel>3){ CERR << TMP1 << '\n'; }
	  CERR << CLOCK()*1e-6 << " mod reduce end, remainder size " << TMP1.coord.size() << " begin gbasis update" << '\n';
	}
	if (!TMP1.coord.empty()){
	  increase(res);
	  if (ressize==res.size())
	    res.push_back(polymod<tdeg_t,modint_t>(TMP1.order,TMP1.dim));
	  swap(res[ressize].coord,TMP1.coord);
	  ++ressize;
#if GBASIS_POSTF4BUCHBERGER==0
	  // this does not warrant full interreduced answer
	  // because at the final step we assume that each spoly
	  // is reduced by the previous spolys in res
	  // either by the first reduction or by inter-reduction
	  // here at most GBASISF4_BUCHBERGER spolys may not be reduced by the previous ones
	  // it happens for example for cyclic8, element no 97
	  gbasis_updatemod(G,B,res,ressize-1,TMP2,env,false,oldG);
#else
	  gbasis_updatemod(G,B,res,ressize-1,TMP2,env,true,oldG);
#endif
	  if (debug_infolevel>3)
	    CERR << CLOCK()*1e-6 << " mod basis indexes " << G << " pairs indexes " << B << '\n';
	}
	else {
	  if (learning && pairs_reducing_to_zero){
	    if (debug_infolevel>2)
	      CERR << "learning " << bk << '\n';
	    pairs_reducing_to_zero->push_back(bk);
	  }
	}
	continue;
      }
      vector< paire > smallposp;
      if (smallposv.size()==B.size()){
	swap(smallposp,B);
	B.clear();
      }
      else {
	for (unsigned i=0;i<smallposv.size();++i)
	  smallposp.push_back(B[smallposv[i]]);
	// remove pairs
	for (int i=int(smallposv.size())-1;i>=0;--i)
	  B.erase(B.begin()+smallposv[i]);
      }
      vectpolymod<tdeg_t,modint_t> f4buchbergerv; // collect all spolys
      int f4res=-1;
      f4res=f4mod<tdeg_t,modint_t,modint2>(res,G,env,smallposp,f4buchbergerv,learning,learned_position,pairs_reducing_to_zero,f4buchberger_info,f4buchberger_info_position,recomputeR);
      if (f4res==-1)
	return false;
      if (f4res==0)
	continue;
      // update gbasis and learning
      // requires that Gauss pivoting does the same permutation for other primes
      if (learning && pairs_reducing_to_zero){
	for (unsigned i=0;i<f4buchbergerv.size();++i){
	  if (f4buchbergerv[i].coord.empty()){
	    if (debug_infolevel>2)
	      CERR << "learning f4buchberger " << smallposp[i] << '\n';
	    pairs_reducing_to_zero->push_back(smallposp[i]);
	  }
	}
      }
      unsigned added=0;
      for (unsigned i=0;i<f4buchbergerv.size();++i){
	if (!f4buchbergerv[i].coord.empty())
	  ++added;
      }
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " reduce f4buchberger end on " << added << " from " << f4buchbergerv.size() << " pairs, gbasis update begin" << '\n';
      for (unsigned i=0;i<f4buchbergerv.size();++i){
	if (!f4buchbergerv[i].coord.empty()){
	  increase(res);
	  if (ressize==res.size())
	    res.push_back(polymod<tdeg_t,modint_t>(TMP1.order,TMP1.dim));
	  swap(res[ressize].coord,f4buchbergerv[i].coord);
	  ++ressize;
#ifdef GBASIS_POSTF4BUCHBERGER
#if GBASIS_POSTF4BUCHBERGER==0
	  if (learning || !f4buchberger_info || f4buchberger_info_position-1>=f4buchberger_info->size())
	    gbasis_updatemod(G,B,res,ressize-1,TMP2,env,false,oldG);
#else
	  gbasis_updatemod(G,B,res,ressize-1,TMP2,env,added<=GBASISF4_BUCHBERGER,oldG);
#endif
#else
	  gbasis_updatemod(G,B,res,ressize-1,TMP2,env,true,oldG);
#endif
	}
	else {
	  // if (!learning && pairs_reducing_to_zero)  CERR << " error learning "<< '\n';
	}
      }
#if GBASIS_POSTF4BUCHBERGER==0
      if (!learning && f4buchberger_info && f4buchberger_info_position-1<f4buchberger_info->size()){
	B=(*f4buchberger_info)[f4buchberger_info_position-1].B;
	G=(*f4buchberger_info)[f4buchberger_info_position-1].G;
	continue;
      }
      if (learning && f4buchberger_info){
	f4buchberger_info->back().B=B;
	f4buchberger_info->back().G=G;
	f4buchberger_info->back().nonzero=added;
      }
#endif
	  unsigned debut = unsigned(G.size()) - added;
#if GBASIS_POSTF4BUCHBERGER>0
      if (added>GBASISF4_BUCHBERGER){
	// final interreduce 
	vector<unsigned> G1(G.begin(),G.begin()+debut);
	vector<unsigned> G2(G.begin()+debut,G.end());
	vector<int> permu2;
	if (!learning && f4buchberger_info){
	  const info_t<tdeg_t,modint_t> & info=(*f4buchberger_info)[f4buchberger_info_position-1];
	  rref_f4buchbergermod_nointerreduce(res,G1,res,G2,-1,info.quo2,info.R2,env,permu2);
	}
	else {
	  information.R2.order=TMP1.order;
	  information.R2.dim=TMP1.dim;
	  TMP1.coord.clear();
	  if (debug_infolevel>1)
	    CERR << CLOCK()*1e-6 << " collect monomials from old basis" << '\n';
	  collect(res,G1,TMP1); // collect all monomials in res[G[0..debut-1]]
	  // in_heap_reducemod(TMP1,res,G2,-1,info_tmp.quo2,TMP2,&info_tmp.R2,env);
	  in_heap_reducemod(TMP1,res,G2,-1,information.quo2,TMP2,&information.R2,env);
	  rref_f4buchbergermod_nointerreduce(res,G1,res,G2,-1,information.quo2,information.R2,env,permu2);
	  if (f4buchberger_info){
	    info_t<tdeg_t,modint_t> & i=f4buchberger_info->back();
	    swap(i.quo2,information.quo2);
	    swap(i.R2.coord,information.R2.coord);
	    i.R2.order=TMP1.order;
	    i.R2.dim=TMP1.dim;
	  }
	}
      }
#endif
      // CERR << "finish loop G.size "<<G.size() << '\n';
      // CERR << added << '\n';
    }
#if GBASIS_POSTF4BUCHBERGER==0
    // final interreduce step2
    for (unsigned j=0; j<G.size();++j){
      reducesmallmod(res[G[j]],res,G,j,env,TMP2,true);
    }
#endif
    if (ressize<res.size())
      res.resize(ressize);
    if (debug_infolevel>1){
      unsigned t=0;
      for (unsigned i=0;i<res.size();++i)
	t += unsigned(res[i].coord.size());
      CERR << CLOCK()*1e-6 << " total number of monomials in res " << t << '\n';
      CERR << "Number of monomials cleared " << cleared << '\n';
    }
    // sort(res.begin(),res.end(),tripolymod<tdeg_t,modint_t>);
    return true;
  }
#endif // GBASISF4_BUCHBERGER

  template<class tdeg_t>
  bool in_gbasisf4buchbergermod(vectpoly8<tdeg_t> & res8,vectpolymod<tdeg_t,modint> &res,vector<unsigned> & G,modint env,bool totdeg,vector< paire > * pairs_reducing_to_zero,vector< info_t<tdeg_t,modint> > * f4buchberger_info,bool recomputeR){
    convert(res8,res,env);
    unsigned ressize = unsigned(res8.size());
    bool b=in_gbasisf4buchbergermod(res,ressize,G,env,totdeg,pairs_reducing_to_zero,f4buchberger_info,recomputeR);
    convert(res,res8,env);
    return b;
  }

  template<class tdeg_t>
  bool in_gbasisf4buchbergermod(vectpoly8<tdeg_t> & res8,vectpolymod<tdeg_t,mod4int> &res,vector<unsigned> & G,mod4int env,bool totdeg,vector< paire > * pairs_reducing_to_zero,vector< info_t<tdeg_t,mod4int> > * f4buchberger_info,bool recomputeR){
    return false;
  }

  // set P mod p*q to be chinese remainder of P mod p and Q mod q 
  template<class tdeg_t>
  bool chinrem(poly8<tdeg_t> &P,const gen & pmod,poly8<tdeg_t> & Q,const gen & qmod,poly8<tdeg_t> & tmp){
    gen u,v,d,pqmod(pmod*qmod);
    egcd(pmod,qmod,u,v,d);
    if (u.type==_ZINT && qmod.type==_INT_)
      u=modulo(*u._ZINTptr,qmod.val);
    if (d==-1){ u=-u; v=-v; d=1; }
    if (d!=1)
      return false;
    int qmodval=0,U=0;
    mpz_t tmpz;
    mpz_init(tmpz);
    if (qmod.type==_INT_ && u.type==_INT_ && pmod.type==_ZINT){
      qmodval=qmod.val;
      U=u.val;
    }
    typename vector< T_unsigned<gen,tdeg_t> >::iterator it=P.coord.begin(),itend=P.coord.end(),jt=Q.coord.begin(),jtend=Q.coord.end();
    if (P.coord.size()==Q.coord.size()){
#ifndef USE_GMP_REPLACEMENTS
      if (qmodval){
	for (;it!=itend;++it,++jt){
	  if (it->u!=jt->u || jt->g.type!=_INT_)
	    break;
	}
	if (it==itend){
	  for (it=P.coord.begin(),jt=Q.coord.begin();it!=itend;++jt,++it){
	    if (it->g.type==_ZINT){
	      mpz_set_si(tmpz,jt->g.val);
	      mpz_sub(tmpz,tmpz,*it->g._ZINTptr);
	      mpz_mul_si(tmpz,*pmod._ZINTptr,(extend(U)*modulo(tmpz,qmodval))%qmodval);
	      mpz_add(*it->g._ZINTptr,*it->g._ZINTptr,tmpz);
	    }
	    else {
	      mpz_mul_si(tmpz,*pmod._ZINTptr,(U*(extend(jt->g.val)-it->g.val))%qmodval);
	      if (it->g.val>=0)
		mpz_add_ui(tmpz,tmpz,it->g.val);
	      else
		mpz_sub_ui(tmpz,tmpz,-it->g.val);
	      it->g=tmpz;
	    }
	  }
	  return true;
	}
	else {
	  if (debug_infolevel)
	    CERR << "warning chinrem: exponent mismatch " << it->u << "," << jt->u << '\n';
	}
      }
#endif
    }
    else {
      if (debug_infolevel)
	CERR << "warning chinrem: sizes differ " << P.coord.size() << "," << Q.coord.size() << '\n';
    }
    tmp.coord.clear(); tmp.dim=P.dim; tmp.order=P.order;
    tmp.coord.reserve(P.coord.size()+3); // allow 3 more terms in Q without realloc
    for (it=P.coord.begin(),jt=Q.coord.begin();it!=itend && jt!=jtend;){
      if (it->u==jt->u){
	gen g;
#ifndef USE_GMP_REPLACEMENTS
	if (qmodval && jt->g.type==_INT_){
	  if (it->g.type==_ZINT){
	    mpz_set_si(tmpz,jt->g.val);
	    mpz_sub(tmpz,tmpz,*it->g._ZINTptr);
	    mpz_mul_si(tmpz,*pmod._ZINTptr,(extend(U)*modulo(tmpz,qmodval))%qmodval);
	    mpz_add(tmpz,tmpz,*it->g._ZINTptr);
	  }
	  else {
	    mpz_mul_si(tmpz,*pmod._ZINTptr,(U*(extend(jt->g.val)-it->g.val))%qmodval);
	    if (it->g.val>=0)
	      mpz_add_ui(tmpz,tmpz,it->g.val);
	    else
	      mpz_sub_ui(tmpz,tmpz,-it->g.val);
	  }
	  g=tmpz;
	}
	else
#endif
	  g=it->g+u*(jt->g-it->g)*pmod;
	tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),it->u));
	++it; ++jt;
	continue;
      }
      if (tdeg_t_strictly_greater(it->u,jt->u,P.order)){
	if (debug_infolevel)
	  CERR << "chinrem: exponent mismatch using first " << '\n';
	gen g=it->g-u*(it->g)*pmod;
	tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),it->u));
	++it;
      }
      else {
	if (debug_infolevel)
	  CERR << "chinrem: exponent mismatch using second " << '\n';
	gen g=u*(jt->g)*pmod;
	tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),jt->u));
	++jt;
      }
    }
    if (it!=itend && debug_infolevel)
      CERR << "chinrem (gen): exponent mismatch at end using first, # " << itend-it << '\n';
    for (;it!=itend;++it){
      gen g=it->g-u*(it->g)*pmod;
      tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),it->u));
    }
    if (jt!=jtend && debug_infolevel)
      CERR << "chinrem (gen): exponent mismatch at end using second " << jtend-jt << '\n';
    for (;jt!=jtend;++jt){
      gen g=u*(jt->g)*pmod;
      tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),jt->u));
    }
    swap(P.coord,tmp.coord);
    mpz_clear(tmpz);
    return true;
  }

  // set P mod p*q to be chinese remainder of P mod p and Q mod q
  // P and Q must have same leading monomials,
  // otherwise returns 0 and leaves P unchanged
  template<class tdeg_t>
  int chinrem(vectpoly8<tdeg_t> &P,const gen & pmod,vectpoly8<tdeg_t> & Q,const gen & qmod,poly8<tdeg_t> & tmp){
    if (P.size()!=Q.size())
      return 0;
    for (unsigned i=0;i<P.size();++i){
      if (P[i].coord.front().u!=Q[i].coord.front().u)
	return 0;
    }
    // LP(P)==LP(Q), proceed to chinese remaindering
    for (unsigned i=0;i<P.size();++i){
      if (!chinrem(P[i],pmod,Q[i],qmod,tmp))
	return -1;
    }
    return 1;
  }

  // parallel chinrem
  
  template<class tdeg_t,class modint_t>
  struct chinrem_t {
    poly8<tdeg_t> * Pptr;
    const polymod<tdeg_t,modint_t> * Qptr;
    size_t start,end;
    int U,qmodval;
    mpz_t * pmodptr,*tmpzptr;
  };

  template<class tdeg_t,class modint_t>
  void * thread_chinrem(void * _ptr){
    chinrem_t<tdeg_t,modint_t> * ptr=(chinrem_t<tdeg_t,modint_t> *) _ptr;
    poly8<tdeg_t> & P=*ptr->Pptr;
    const polymod<tdeg_t,modint_t> & Q=*ptr->Qptr;
    size_t start=ptr->start,end=ptr->end;
    if (end>P.coord.size())
      end=P.coord.size();
    int U=ptr->U,qmodval=ptr->qmodval;
    mpz_t * pmodptr=ptr->pmodptr,*tmpzptr=ptr->tmpzptr;
    typename vector< T_unsigned<gen,tdeg_t> >::iterator it=P.coord.begin()+start,itend=P.coord.begin()+end;
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=Q.coord.begin()+start;
    for (;it!=itend;++it,++jt){
      if (it->g.type==_ZINT){
        int amodq=modulo(*it->g._ZINTptr,qmodval);
        if (amodq==jt->g)
          continue;
        mpz_mul_si(*tmpzptr,*pmodptr,(U*(jt->g-extend(amodq)))%qmodval);
        mpz_add(*it->g._ZINTptr,*it->g._ZINTptr,*tmpzptr);
      }
      else {
        mpz_mul_si(*tmpzptr,*pmodptr,(U*(extend(jt->g)-it->g.val))%qmodval);
        if (it->g.val>=0)
          mpz_add_ui(*tmpzptr,*tmpzptr,it->g.val);
        else
          mpz_sub_ui(*tmpzptr,*tmpzptr,-it->g.val);
        it->g=*tmpzptr;
      }
    }
    return ptr;
  }

  // set P mod p*q to be chinese remainder of P mod p and Q mod q
  template<class tdeg_t,class modint_t>
  bool chinrem(poly8<tdeg_t> &P,const gen & pmod,const polymod<tdeg_t,modint_t> & Q,int qmodval,poly8<tdeg_t> & tmp,int nthreads=1){
    if (pmod.type!=_ZINT)
      nthreads=1;
    if (pmod.type!=_ZINT)
      nthreads=1;
    else {
      double work=P.coord.size();
      work=work*sizeinbase2(pmod)/256/29; // correction of number of monomials by a factor corresponding to 256 primes of size 29 bits
      if (work/nthreads<128)
        nthreads=giacmin(nthreads,giacmax(1,work/128));
    }
    if (nthreads>MAXNTHREADS)
      nthreads=MAXNTHREADS;
    gen u,v,d,pqmod(qmodval*pmod);
    egcd(pmod,qmodval,u,v,d);
    if (u.type==_ZINT)
      u=modulo(*u._ZINTptr,qmodval);
    if (d==-1){ u=-u; v=-v; d=1; }
    if (d!=1)
      return false;
    int U=u.val;
    mpz_t tmpz;
    mpz_init(tmpz);
    typename vector< T_unsigned<gen,tdeg_t> >::iterator it=P.coord.begin(),itend=P.coord.end();
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=Q.coord.begin(),jtend=Q.coord.end();
#ifndef USE_GMP_REPLACEMENTS
    if (P.coord.size()==Q.coord.size()){
      for (;it!=itend;++it,++jt){
	if (it->u!=jt->u)
	  break;
      }
      if (it==itend){
#if !defined USE_GMP_REPLACEMENTS && defined HAVE_LIBPTHREAD
        if (//0 &&
            nthreads>1 
            // && P.coord.size()>=64*nthreads
            ){
          // realloc mpz_t inside P ?
          size_t cur=sizeinbase2(pqmod);
          int N=sizeinbase2(cur)-1;
          size_t N2=(1ULL << N);
          gen N3=pow(2,N2+31);
          if (is_greater(N3,pqmod,context0)){
            if (debug_infolevel>1)
              CERR << CLOCK()*1e-6 << " parallel chinrem realloc bits=" << 2*N2 << "\n";
            for (it=P.coord.begin();it!=itend;++it){
              if (it->g.type!=_ZINT) continue;
              mpz_t & z=*it->g._ZINTptr;
              size_t taille=mpz_size(z)*sizeof(mp_limb_t)*8;
              if (taille<2*N2)
                mpz_realloc2(z,2*N2);
            }
          }
          // parallel chinese remaindering 
          chinrem_t<tdeg_t,modint_t> chinrem_param[MAXNTHREADS]; mpz_t tmptab[MAXNTHREADS];
          pthread_t tab[MAXNTHREADS];
          for (int j=0;j<nthreads;++j){
            chinrem_t<tdeg_t,modint_t> tmp={&P,&Q,j*P.coord.size()/nthreads,(j+1)*P.coord.size()/nthreads,U,qmodval,pmod._ZINTptr,&tmptab[j]};
            chinrem_param[j]=tmp;
            mpz_init2(tmptab[j],cur);
            bool res=true;
            if (j<nthreads-1)
              res=pthread_create(&tab[j],(pthread_attr_t *) NULL,thread_chinrem<tdeg_t,modint_t>,(void *) &chinrem_param[j]);
            if (res)
              thread_chinrem<tdeg_t,modint_t>((void *)&chinrem_param[j]);
          } // end creating threads
          for (unsigned j=0;j<nthreads;++j){
            void * ptr_=(void *)&nthreads; // non-zero initialisation
            if (j<nthreads-1) pthread_join(tab[j],&ptr_);
            mpz_clear(tmptab[j]); 
          }
          mpz_clear(tmpz);
          return true;
        }
#endif
	for (it=P.coord.begin(),jt=Q.coord.begin();it!=itend;++jt,++it){
	  if (pmod.type!=_ZINT){
	    it->g=it->g+u*(jt->g-it->g)*pmod;
	    continue;
	  }
	  if (it->g.type==_ZINT){
#if 1
	    int amodq=modulo(*it->g._ZINTptr,qmodval);
	    if (amodq==jt->g)
	      continue;
	    mpz_mul_si(tmpz,*pmod._ZINTptr,(U*(jt->g-extend(amodq)))%qmodval);
	    mpz_add(*it->g._ZINTptr,*it->g._ZINTptr,tmpz);
#else
	    mpz_set_si(tmpz,jt->g);
	    mpz_sub(tmpz,tmpz,*it->g._ZINTptr);
	    mpz_mul_si(tmpz,*pmod._ZINTptr,(extend(U)*modulo(tmpz,qmodval))%qmodval);
	    mpz_add(*it->g._ZINTptr,*it->g._ZINTptr,tmpz);
#endif
	  }
	  else {
	    mpz_mul_si(tmpz,*pmod._ZINTptr,(U*(extend(jt->g)-it->g.val))%qmodval);
	    if (it->g.val>=0)
	      mpz_add_ui(tmpz,tmpz,it->g.val);
	    else
	      mpz_sub_ui(tmpz,tmpz,-it->g.val);
	    it->g=tmpz;
	  }
	}
	mpz_clear(tmpz);
	return true;
      }
    }
#endif
    tmp.coord.clear(); tmp.dim=P.dim; tmp.order=P.order;
    tmp.coord.reserve(P.coord.size()+3); // allow 3 more terms in Q without realloc
    for (it=P.coord.begin(),jt=Q.coord.begin();it!=itend && jt!=jtend;){
      if (it->u==jt->u){
	gen g;
#ifndef USE_GMP_REPLACEMENTS
	if (pmod.type==_ZINT){
	  if (it->g.type==_ZINT){
	    mpz_set_si(tmpz,jt->g);
	    mpz_sub(tmpz,tmpz,*it->g._ZINTptr);
	    mpz_mul_si(tmpz,*pmod._ZINTptr,(extend(U)*modulo(tmpz,qmodval))%qmodval);
	    mpz_add(tmpz,tmpz,*it->g._ZINTptr);
	  }
	  else {
	    mpz_mul_si(tmpz,*pmod._ZINTptr,(U*(extend(jt->g)-it->g.val))%qmodval);
	    if (it->g.val>=0)
	      mpz_add_ui(tmpz,tmpz,it->g.val);
	    else
	      mpz_sub_ui(tmpz,tmpz,-it->g.val);
	  }
	  g=tmpz;
	}
	else
#endif
	  g=it->g+u*(jt->g-it->g)*pmod;
	tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),it->u));
	++it; ++jt;
	continue;
      }
      if (tdeg_t_strictly_greater(it->u,jt->u,P.order)){
	if (debug_infolevel)
	  CERR << "chinrem: exponent mismatch using first " << '\n';
	gen g=it->g-u*(it->g)*pmod;
	tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),it->u));
	++it;
      }
      else {
	if (debug_infolevel)
	  CERR << "chinrem: exponent mismatch using second " << '\n';
	gen g=u*((jt->g)*pmod);
	tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),jt->u));
	++jt;
      }
    }
    if (it!=itend && debug_infolevel)
      CERR << "chinrem (int): exponent mismatch at end using first #" << itend-it << '\n';
    for (;it!=itend;++it){
      gen g=it->g-u*(it->g)*pmod;
      tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),it->u));
    }
    if (jt!=jtend && debug_infolevel)
      CERR << "chinrem (int): exponent mismatch at end using second #" << jtend-jt << '\n';
    for (;jt!=jtend;++jt){
      gen g=u*((jt->g)*pmod);
      tmp.coord.push_back(T_unsigned<gen,tdeg_t>(smod(g,pqmod),jt->u));
    }
    swap(P.coord,tmp.coord);
    mpz_clear(tmpz);
    return true;
  }

  // set P mod p*q to be chinese remainder of P mod p and Q mod q
  // P and Q must have same leading monomials,
  // otherwise returns 0 and leaves P unchanged
  template<class tdeg_t,class modint_t>
  int chinrem(vectpoly8<tdeg_t> &P,const gen & pmod,const vectpolymod<tdeg_t,modint_t> & Q,int qmod,poly8<tdeg_t> & tmp,int start=0,int nthreads=1){
    if (P.size()!=Q.size())
      return 0;
    for (unsigned i=start;i<P.size();++i){
      if (P[i].coord.empty() && Q[i].coord.empty())
	continue;
      if (P[i].coord.empty())
	return 0;
      if (Q[i].coord.empty())
	return 0;
      if (P[i].coord.front().u!=Q[i].coord.front().u)
	return 0;
    }
    // LP(P)==LP(Q), proceed to chinese remaindering
    for (unsigned i=start;i<P.size();++i){
      if (!chinrem(P[i],pmod,Q[i],qmod,tmp,nthreads))
	return -1;
    }
    return 1;
  }

  // a mod b = r/u with n and d<sqrt(b)/2
  // a*u = r mod b -> a*u+b*v=r, Bezout with a and b
  bool fracmod(int a,int b,int & n,int & d){
    if (a<0){
      if (!fracmod(-a,b,n,d))
	return false;
      n=-n;
      return true;
    }
    int r=b,u=0; // v=1
    int r1=a,u1=1,r2,u2,q; // v1=0
    for (;double(2*r1)*r1>b;){
      q=r/r1;
      u2=u-q*u1;
      r2=r-q*r1;
      u=u1;
      u1=u2;
      r=r1;
      r1=r2;
    }
    if (double(2*u1)*u1>b)
      return false;
    if (u1<0){ u1=-u1; r1=-r1; }
    n=r1; d=u1;
    return true;
  }

  // search for d such that d*P mod p has small coefficients
  // call with d set to 1,
  template<class tdeg_t>
  bool findmultmod(const poly8<tdeg_t> & P,int p,int & d){
    int n,s=int(P.coord.size());
    for (int i=0;i<s;++i){
      int a=smod(extend(P.coord[i].g.val)*d,p);
      if (double(2*a)*a<p)
	continue;
      int d1=1;
      if (!fracmod(a,p,n,d1) || double(2*d1)*d1>p){
	if (debug_infolevel)
	  COUT << "findmultmod failure " << a << " mod " << p << '\n';
	return false;
      }
      d=d*d1;
    }
    if (debug_infolevel){
      for (int i=0;i<s;++i){
	int a=smod(extend(P.coord[i].g.val)*d,p);
	if (double(2*a)*a>=p){
	  COUT << "possible findmultmod failure " << P.coord[i].g.val << " " << d << " " << a << " " << p << '\n';
	  //return false;
	}
      }
    }
    return true;
  }

  bool chk_equal_mod(const gen & a,const vector<int> & p,int m){
    if (a.type!=_VECT || a._VECTptr->size()!=p.size())
      return false;
    const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
    vector<int>::const_iterator jt=p.begin();
    for (;it!=itend;++jt,++it){
      if (it->type==_INT_ && it->val==*jt) continue;
      if (!chk_equal_mod(*it,*jt,m))
	return false;
    }
    return true;
  }

  bool chk_equal_mod(const vecteur & v,const vector< vector<int> >& p,int m){
    if (v.size()!=p.size())
      return false;
    for (unsigned i=0;i<p.size();++i){
      if (!chk_equal_mod(v[i],p[i],m))
	return false;
    }
    return true;
  }

  template<class tdeg_t,class modint_t>
  bool chk_equal_mod(const poly8<tdeg_t> & v,const polymod<tdeg_t,modint_t> & p,int m){
    // sizes may differ if a coeff of v is 0 mod m
    if (v.coord.size()<p.coord.size())
      return false;
    if (v.coord.empty())
      return true;
    unsigned s = unsigned(v.coord.size());
    int lc=smod(v.coord[0].g,m).val;
    int lcp=p.coord.empty()?1:p.coord[0].g;
    if (s==p.coord.size()){
      if (lcp!=1){
	for (unsigned i=0;i<s;++i){
	  if (!chk_equal_mod(lcp*v.coord[i].g,(extend(lc)*p.coord[i].g)%m,m))
	    return false;
	}
      }
      else {
	for (unsigned i=0;i<s;++i){
	  if (!chk_equal_mod(v.coord[i].g,(extend(lc)*p.coord[i].g)%m,m))
	    return false;
	}
      }
      return true;
    }
    unsigned j=0; // position in p
    for (unsigned i=0;i<s;++i){
      if (v.coord[i].u==p.coord[j].u){
	if (!chk_equal_mod(lcp*v.coord[i].g,(extend(lc)*p.coord[j].g)%m,m))
	  return false;
	++j;
      }
      if (!chk_equal_mod(lcp*v.coord[i].g,0,m))
	return false;
    }
    return true;
  }

  template<class tdeg_t,class modint_t>
  bool chk_equal_mod(const vectpoly8<tdeg_t> & v,const vectpolymod<tdeg_t,modint_t> & p,const vector<unsigned> & G,int m){
    if (v.size()!=G.size())
      return false;
    for (unsigned i=0;i<G.size();++i){
      if (!chk_equal_mod(v[i],p[G[i]],m))
	return false;
    }
    return true;
  }

  template<class tdeg_t>
  bool chk_equal_mod(const poly8<tdeg_t> & v,const poly8<tdeg_t> & p,int m){
    if (v.coord.size()!=p.coord.size())
      return false;
    unsigned s=unsigned(p.coord.size());
    int lc=smod(v.coord[0].g,m).val;
    for (unsigned i=0;i<s;++i){
      if (!chk_equal_mod(v.coord[i].g,(extend(lc)*p.coord[i].g.val)%m,m))
	return false;
    }
    return true;
  }

  template<class tdeg_t>
  bool chk_equal_mod(const vectpoly8<tdeg_t> & v,const vectpoly8<tdeg_t> & p,const vector<unsigned> & G,int m){
    if (v.size()!=G.size())
      return false;
    for (unsigned i=0;i<G.size();++i){
      if (!chk_equal_mod(v[i],p[G[i]],m))
	return false;
    }
    return true;
  }

  template<class tdeg_t,class modint_t>
  int fracmod(const poly8<tdeg_t> &P,const gen & p,
	       mpz_t & d,mpz_t & d1,mpz_t & absd1,mpz_t &u,mpz_t & u1,mpz_t & ur,mpz_t & q,mpz_t & r,mpz_t &sqrtm,mpz_t & tmp,
	       poly8<tdeg_t> & Q,
               polymod<tdeg_t,modint_t> * chkptr=0,int chkp=0){
    Q.coord.clear();
    Q.coord.reserve(P.coord.size());
    Q.dim=P.dim;
    Q.order=P.order;
    Q.sugar=P.sugar;
    gen L=1; 
    bool tryL=true;
    for (unsigned i=0;i<P.coord.size();++i){
      gen g=P.coord[i].g,num,den;
      if (g.type==_INT_)
	g.uncoerce();
      if ( (g.type!=_ZINT) || (p.type!=_ZINT) ){
	CERR << "bad type"<<'\n';
	return 0;
      }
      if (tryL && L.type==_ZINT){
	num=smod(L*g,p);
	if (is_greater(p,4*num*num,context0)){
	  g=fraction(num,L);
	  Q.coord.push_back(T_unsigned<gen,tdeg_t>(g,P.coord[i].u));
	  continue;
	}
      }
      if (!in_fracmod(p,g,d,d1,absd1,u,u1,ur,q,r,sqrtm,tmp,num,den))
	return 0;
      if (num.type==_ZINT && mpz_sizeinbase(*num._ZINTptr,2)<=30)
	num=int(mpz_get_si(*num._ZINTptr));
      if (den.type==_ZINT && mpz_sizeinbase(*den._ZINTptr,2)<=30)
	den=int(mpz_get_si(*den._ZINTptr));
      if (!is_positive(den,context0)){ // ok
	den=-den;
	num=-num;
      }
      g=fraction(num,den);
      if (tryL){
	L=lcm(L,den);
	tryL=is_greater(p,L*L,context0);
      }
      Q.coord.push_back(T_unsigned<gen,tdeg_t>(g,P.coord[i].u));
      if (chkptr && !chk_equal_mod(g,chkptr->coord[i].g,chkp)){
        if (debug_infolevel)
          CERR << CLOCK()*1e-6 << " early fracmod chk failure position " << i << " (" << P.coord.size() << ")\n";
        return 2;
      }
    }
    return 1;
  }

  template<class tdeg_t>
  bool fracmod(const vectpoly8<tdeg_t> & P,const gen & p_,
		      mpz_t & d,mpz_t & d1,mpz_t & absd1,mpz_t &u,mpz_t & u1,mpz_t & ur,mpz_t & q,mpz_t & r,mpz_t &sqrtm,mpz_t & tmp,
		      vectpoly8<tdeg_t> & Q){
    Q.resize(P.size());
    gen p=p_;
    if (p.type==_INT_)
      p.uncoerce();
    bool ok=true;
    for (unsigned i=0;i<P.size();++i){
      if (!fracmod(P[i],p,d,d1,absd1,u,u1,ur,q,r,sqrtm,tmp,Q[i])){
	ok=false;
	break;
      }
    }
    return ok;
  }

  template <class tdeg_t>
  void cleardeno(poly8<tdeg_t> &P){
    gen g=1;
    for (unsigned i=0;i<P.coord.size();++i){
      if (P.coord[i].g.type==_FRAC)
	g=lcm(g,P.coord[i].g._FRACptr->den);
    }
    if (g!=1){
      for (unsigned i=0;i<P.coord.size();++i){
	P.coord[i].g=g*P.coord[i].g;
      }
    }
  }

  template<class tdeg_t>
  void cleardeno(vectpoly8<tdeg_t> & P){
    if (debug_infolevel)
      COUT << "clearing denominators of revlex gbasis ";
    for (unsigned i=0;i<P.size();++i){
      cleardeno(P[i]);
      if (debug_infolevel){
	if (i%10==9){
	  COUT << "+";
	  COUT.flush();
	}
	if (i%500==499) 
	  COUT << " " << CLOCK()*1e-6 << " remaining " << P.size()-i << '\n';
      }
    }
    if (debug_infolevel)
      COUT << " done\n";
  }


  template<class tdeg_t,class modint_t>
  void collect(const vectpoly8<tdeg_t> & f4buchbergerv,polymod<tdeg_t,modint_t> & allf4buchberger){
    typename vectpoly8<tdeg_t>::const_iterator it=f4buchbergerv.begin(),itend=f4buchbergerv.end();
    vector<heap_tt<tdeg_t> > H;
    H.reserve(itend-it);
    order_t keyorder={_REVLEX_ORDER,0};
    for (unsigned i=0;it!=itend;++i,++it){
      keyorder=it->order;
      if (!it->coord.empty())
	H.push_back(heap_tt<tdeg_t>(i,0,it->coord.front().u));
    }
    compare_heap_tt<tdeg_t> key(keyorder);
    make_heap(H.begin(),H.end(),key);
    while (!H.empty()){
      std::pop_heap(H.begin(),H.end(),key);
      // push root node of the heap in allf4buchberger
      heap_tt<tdeg_t> & current =H.back();
      if (allf4buchberger.coord.empty() || allf4buchberger.coord.back().u!=current.u)
	allf4buchberger.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,current.u));
      ++current.polymodpos;
      if (current.polymodpos>=f4buchbergerv[current.f4buchbergervpos].coord.size()){
	H.pop_back();
	continue;
      }
      current.u=f4buchbergerv[current.f4buchbergervpos].coord[current.polymodpos].u;
      std::push_heap(H.begin(),H.end(),key);
    }
  }

  template<class tdeg_t,class modint_t>
  void makeline(const poly8<tdeg_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vecteur & v){
    v=vecteur(R.coord.size(),0);
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=R.coord.begin(),jtbeg=jt,jtend=R.coord.end();
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v[jt-jtbeg]=it->g;
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v[jt-jtbeg]=it->g;
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  unsigned firstnonzero(const vecteur & v){
    for (unsigned i=0;i<v.size();++i){
      if (v[i]!=0)
	return i;
    }
    return unsigned(v.size());
  }

  struct sparse_gen {
    gen val;
    unsigned pos;
    sparse_gen(const gen & v,unsigned u):val(v),pos(u){};
    sparse_gen():val(0),pos(-1){};
  };

  template<class tdeg_t,class modint_t>
  void makeline(const poly8<tdeg_t> & p,const tdeg_t * shiftptr,const polymod<tdeg_t,modint_t> & R,vector<sparse_gen> & v){
    typename std::vector< T_unsigned<gen,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=R.coord.begin(),jtend=R.coord.end();
    if (shiftptr){
      for (;it!=itend;++it){
	tdeg_t u=it->u+*shiftptr;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v.push_back(sparse_gen(it->g,int(jt-R.coord.begin())));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=it->u;
	for (;jt!=jtend;++jt){
	  if (jt->u==u){
	    v.push_back(sparse_gen(it->g,int(jt-R.coord.begin())));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

#ifdef x86_64
  bool checkreducef4buchberger_64(vector<modint> &v,vector<modint> & coeff,const vector< vector<sparse_element> > & M,modint env,vector<int128_t> & w){
    w.resize(v.size());
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    vector<int128_t>::iterator wt=w.begin();
    for (;vt!=vtend;++wt,++vt){
      *wt=*vt;
    }
    for (unsigned i=0;i<M.size();++i){
      const vector<sparse_element> & m=M[i];
      const sparse_element * it=&m.front(),*itend=it+m.size(),*it2;
      if (it==itend)
	continue;
      int128_t & ww=w[it->pos];
      if (ww==0){
	coeff[i]=0;
	continue;
      }
      modint c=coeff[i]=(extend(invmod(it->val,env))*ww)%env;
      // CERR << "multiplier ok line " << i << " value " << c << " " << w << '\n';
      if (!c)
	continue;
      ww=0;
      ++it;
      it2=itend-8;
      for (;it<=it2;){
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
	w[it->pos] -= extend(c)*(it->val);
	++it;
      }
      for (;it!=itend;++it){
	w[it->pos] -= extend(c)*(it->val);
      }
    }
    for (vt=v.begin(),wt=w.begin();vt!=vtend;++wt,++vt){
      if (*wt && (*wt % env))
	return false;
    }
    return true;
  }

  bool checkreducef4buchbergersplit_64(vector<modint> &v,vector<modint> & coeff,const vector< vector<shifttype> > & M,vector<vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env,vector<int128_t> & w){
    w.resize(v.size());
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    vector<int128_t>::iterator wt=w.begin(),wtend=w.end();
    for (;vt!=vtend;++wt,++vt){
      *wt=*vt;
    }
    for (unsigned i=0;i<M.size();++i){
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end();
      if (jt==jtend)
	continue;
      const vector<shifttype> & mindex=M[i];
      const shifttype * it=&mindex.front();
      unsigned pos=0;
      next_index(pos,it);
      // if (pos>v.size()) CERR << "error" <<'\n';
      modint c=coeff[i]=(extend(invmod(*jt,env))*w[pos])%env;
      w[pos]=0;
      if (!c)
	continue;
      for (++jt;jt!=jtend;++jt){
#ifdef GIAC_SHORTSHIFTTYPE
	next_index(pos,it);
	int128_t &x=w[pos];
	x -= extend(c)*(*jt);
#else
	w[*it] -= extend(c)*(*jt);
	++it;
#endif
      }
    }
    for (wt=w.begin();wt!=wtend;++wt){
      if (*wt % env)
	return false;
    }
    return true;
  }

#endif

  // return true if v reduces to 0
  // in addition to reducef4buchberger, compute the coeffs
  bool checkreducef4buchberger(vector<modint> &v,vector<modint> & coeff,const vector< vector<sparse_element> > & M,modint env){
    for (unsigned i=0;i<M.size();++i){
      const vector<sparse_element> & m=M[i];
      vector<sparse_element>::const_iterator it=m.begin(),itend=m.end(),it1=itend-8;
      if (it==itend)
	continue;
      modint c=coeff[i]=v[it->pos];
      if (!c)
	continue;
      c=coeff[i]=(extend(invmod(it->val,env))*c)%env;
      v[it->pos]=0;
      for (++it;it<it1;){
	modint *x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
	x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
	x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
	x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
	x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
	x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
	x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
	x=&v[it->pos];
	*x=(*x-extend(c)*(it->val))%env;
	++it;
      }
      for (;it!=itend;++it){
	modint &x=v[it->pos];
	x=(x-extend(c)*(it->val))%env;
      }
    }
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return false;
    }
    return true;
  }

  // return true if v reduces to 0
  // in addition to reducef4buchberger, compute the coeffs
  bool checkreducef4buchbergersplit(vector<modint> &v,vector<modint> & coeff,const vector< vector<shifttype> > & M,vector<vector<modint> > & coeffs,vector<coeffindex_t> & coeffindex,modint env){
    for (unsigned i=0;i<M.size();++i){
      const vector<modint> & mcoeff=coeffs[coeffindex[i].u];
      vector<modint>::const_iterator jt=mcoeff.begin(),jtend=mcoeff.end();
      if (jt==jtend)
	continue;
      const vector<shifttype> & mindex=M[i];
      const shifttype * it=&mindex.front();
      unsigned pos=0;
      next_index(pos,it);
      // if (pos>v.size()) CERR << "error" <<'\n';
      modint c=coeff[i]=(extend(invmod(*jt,env))*v[pos])%env;
      v[pos]=0;
      if (!c)
	continue;
      for (++jt;jt!=jtend;++jt){
#ifdef GIAC_SHORTSHIFTTYPE
	next_index(pos,it);
	modint &x=v[pos];
#else
	modint &x=v[*it];
	++it;
#endif
	x=(x-extend(c)*(*jt))%env;
      }
    }
    vector<modint>::iterator vt=v.begin(),vtend=v.end();
    for (vt=v.begin();vt!=vtend;++vt){
      if (*vt)
	return false;
    }
    return true;
  }

  // Find x=a mod amod and =b mod bmod
  // We have x=a+A*amod=b+B*Bmod
  // hence A*amod-B*bmod=b-a
  // let u*amod+v*bmod=1
  // then A=(b-a)*u is a solution
  // hence x=a+(b-a)*u*amod mod (amod*bmod) is the solution
  // hence x=a+((b-a)*u mod bmod)*amod
  static bool ichinrem_inplace(matrice & a,const vector< vector<modint> > &b,const gen & amod, int bmod){
    gen U,v,d;
    egcd(amod,bmod,U,v,d);
    if (!is_one(d) || U.type!=_ZINT)
      return false;
    int u=mpz_get_si(*U._ZINTptr);
    longlong q;
    for (unsigned i=0;i<a.size();++i){
      gen * ai = &a[i]._VECTptr->front(), * aiend=ai+a[i]._VECTptr->size();
      const modint * bi = &b[i].front();
      for (;ai!=aiend;++bi,++ai){
	if (*bi==0 && ai->type==_INT_ && ai->val==0)
	  continue;
	q=extend(*bi)-(ai->type==_INT_?ai->val:modulo(*ai->_ZINTptr,bmod));
	q=(q*u) % bmod;
	if (amod.type==_ZINT && ai->type==_ZINT){
	  if (q>=0)
	    mpz_addmul_ui(*ai->_ZINTptr,*amod._ZINTptr,int(q));
	  else
	    mpz_submul_ui(*ai->_ZINTptr,*amod._ZINTptr,-int(q));
	}
	else
	  *ai += int(q)*amod;
      }
    }
    return true;
  }

  template<class tdeg_t>
  gen linfnorm(const poly8<tdeg_t> & p,GIAC_CONTEXT){
    gen B=0;
    for (unsigned i=0;i<p.coord.size();++i){
      gen b=abs(p.coord[i].g,contextptr);
      if (is_strictly_greater(b,B,contextptr))
	B=b;
    }
    return B;
  }

  template<class tdeg_t>
  gen linfnorm(const vectpoly8<tdeg_t> & v,GIAC_CONTEXT){
    gen B=0;
    for (unsigned i=0;i<v.size();++i){
      gen b=linfnorm(v[i],contextptr);
      if (is_strictly_greater(b,B,contextptr))
	B=b;
    }
    return B;
  }

  // excluded==-1 and G==identity in calls
  // if eps>0 the check is probabilistic
  template<class tdeg_t,class modint_t>
  bool checkf4buchberger(vectpoly8<tdeg_t> & f4buchbergerv,const vectpoly8<tdeg_t> & res,vector<unsigned> & G,unsigned excluded,double eps){
    if (f4buchbergerv.empty())
      return true;
    polymod<tdeg_t,modint_t> allf4buchberger(f4buchbergerv.front().order,f4buchbergerv.front().dim),rem(allf4buchberger);
    vectpolymod<tdeg_t,modint_t> resmod,quo;
    convert(res,resmod,0);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " checkf4buchberger begin collect monomials on #polys " << f4buchbergerv.size() << '\n';
    // collect all terms in f4buchbergerv
    collect(f4buchbergerv,allf4buchberger);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " checkf4buchberger symbolic preprocess" << '\n';
    // find all monomials required to reduce allf4buchberger with res[G[.]]
    polymod<tdeg_t,modint_t> R;
    in_heap_reducemod(allf4buchberger,resmod,G,excluded,quo,rem,&R,0);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " checkf4buchberger end symbolic preprocess" << '\n';
    // build a matrix with rows res[G[.]]*quo[.] in terms of monomials in allf4buchberger
    // sort the matrix
    // checking reduction to 0 is equivalent to
    // write a line from f4buchbergerv[]
    // as a linear combination of the lines of this matrix
    // we will do that modulo a list of primes
    // and keep track of the coefficients of the linear combination (the quotients)
    // we reconstruct the quotients in Q by fracmod
    // once they stabilize, we compute the lcm l of the denominators
    // we multiply by l to have an equality on Z
    // we compute bounds on the coefficients of the products res*quo
    // and on l*f4buchbergerv, and we check further the equality modulo additional
    // primes until the equality is proved
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin build M" << '\n';
    vector< vector<sparse_gen> > M;
    vector<sparse_element> atrier;
    unsigned N=unsigned(R.coord.size()),i,j=0,nterms=0;
    M.reserve(N); // actual size is at most N (difference is the remainder part size)
    for (i=0;i<res.size();++i){
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      for (;jt!=jtend;++j,++jt){
	M.push_back(vector<sparse_gen>(0));
	makeline(res[G[i]],&jt->u,R,M[j]);
	nterms += unsigned(M[j].size());
	atrier.push_back(sparse_element(M[j].front().pos,j));
      }
    }
    sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1); 
    vector< vector<sparse_gen> > M1(atrier.size());
    for (i=0;i<atrier.size();++i){
      swap(M1[i],M[atrier[i].pos]);
    }
    swap(M,M1);
    // CERR << M << '\n';
    if (debug_infolevel>0)
      CERR << CLOCK()*1e-6 << " rows, columns, terms: " << M.size() << "x" << N << "=" << nterms << '\n'; 
    // PSEUDO_MOD is not interesting here since there is no inter-reduction
    gen p(int(extend(1<<31)-1));
    gen pip(1);
    vectpolymod<tdeg_t,modint_t> f4buchbergervmod;
    matrice coeffmat;
    vector< vector<modint_t> > coeffmatmodp(f4buchbergerv.size(),vector<modint_t>(M.size()));
    gen bres=linfnorm(res,context0);
    gen bf4buchberger=linfnorm(f4buchbergerv,context0);
    matrice prevmatq;
    bool stable=false;
    gen bound=0;
    for (int iter=0;;++iter){
      if (eps>0 && is_greater(eps*pip,1,context0))
	return true;
      p=prevprime(p-1);
      int env=p.val;
      // check that p does not divide a leading monomial in M
      unsigned j;
      for (j=0;j<M.size();++j){
	if (smod(M[j].front().val,p)==0)
	  break;
      }
      if (j<M.size())
	continue;
      // compute M mod p
      vector< vector<sparse_element> > Mp(M.size());
      for (unsigned i=0;i<M.size();++i){
	const vector<sparse_gen> & Mi=M[i];
	vector<sparse_element> Ni;
	Ni.reserve(Mi.size());
	for (unsigned j=0;j<Mi.size();++j){
	  const sparse_gen & Mij=Mi[j];
	  modint_t tmp= Mij.val.type==_ZINT?modulo(*Mij.val._ZINTptr,env):Mij.val.val%env;
	  Ni.push_back(sparse_element(tmp,Mij.pos));
	}
	swap(Mp[i],Ni);
      }
      // reduce f4buchbergerv and stores coefficients of quotients for f4buchbergerv[i] in coeffmat[i]
      convert(f4buchbergerv,f4buchbergervmod,env);
      if (debug_infolevel>0)
	CERR << CLOCK()*1e-6 << " checking mod " << p << '\n';
      vector<modint_t> v;
      unsigned countres=0;
#ifdef x86_64
      vector<int128_t> v128;
#endif
      for (unsigned i=0;i<f4buchbergervmod.size();++i){
	makeline<tdeg_t>(f4buchbergervmod[i],0,R,v);
#if 0 // def x86_64
	if (!checkreducef4buchberger_64(v,coeffmatmodp[i],Mp,env,v128))
	  return false;
#else
	if (!checkreducef4buchberger(v,coeffmatmodp[i],Mp,env))
	  return false;
#endif
	if (iter==0){
	  unsigned countrescur=0;
	  vector<modint_t> & coeffi=coeffmatmodp[i];
	  for (unsigned j=0;j<coeffi.size();++j){
	    if (coeffi[j])
	      ++countrescur;
	  }
	  if (countrescur>countres)
	    countres=countrescur;
	}
      }
      // if (iter==0) bound=pow(bres,int(countres),context0);
      if (stable){
	if (!chk_equal_mod(prevmatq,coeffmatmodp,env))
	  stable=false;
	// if stable compute bounds and compare with product of primes
	// if 2*bounds < product of primes recheck stabilization and return true
	if (is_strictly_greater(pip,bound,context0)){
	  if (debug_infolevel>0)
	    CERR << CLOCK()*1e-6 << " modular check finished " << '\n';
	  return true;
	}
      }
      // combine coeffmat with previous one by chinese remaindering
      if (debug_infolevel>0)
	CERR << CLOCK()*1e-6 << " chinrem mod " << p << '\n';
      if (iter)
	ichinrem_inplace(coeffmat,coeffmatmodp,pip,p.val);
      else
	vectvector_int2vecteur(coeffmatmodp,coeffmat);
      pip=pip*p;
      if (is_greater(bound,pip,context0))
	continue;
      if (!stable){
	// check stabilization 
	matrice checkquo;
	checkquo.reserve(coeffmat.size());
	for (unsigned k=0;k<coeffmat.size();++k){
	  if (prevmatq.size()>k && chk_equal_mod(prevmatq[k],coeffmatmodp[k],env))
	    checkquo.push_back(prevmatq[k]);
	  else
	    checkquo.push_back(fracmod(coeffmat[k],pip));
	  if (prevmatq.size()>k && checkquo[k]!=prevmatq[k])
	    break;
	  if (k>(prevmatq.size()*3)/2+2)
	    break;
	}
	if (checkquo!=prevmatq){
	  swap(prevmatq,checkquo);
	  if (debug_infolevel>0)
	    CERR << CLOCK()*1e-6 << " unstable mod " << p << " reconstructed " << prevmatq.size() << '\n';
	  continue;
	}
	matrice coeffmatq=*_copy(checkquo,context0)._VECTptr;
	if (debug_infolevel>0)
	  CERR << CLOCK()*1e-6 << " full stable mod " << p << '\n';
	stable=true;
	gen lall=1; vecteur l(coeffmatq.size());
	for (unsigned i=0;i<coeffmatq.size();++i){
	  lcmdeno(*coeffmatq[i]._VECTptr,l[i],context0);
	  if (is_strictly_greater(l[i],lall,context0))
	    lall=l[i];
	}
	if (debug_infolevel>0)
	  CERR << CLOCK()*1e-6 << " lcmdeno ok/start bound " << p << '\n';
	gen ball=1,bi; // ball is the max bound of all coeff in coeffmatq
	for (unsigned i=0;i<coeffmatq.size();++i){
	  bi=linfnorm(coeffmatq[i],context0);
	  if (is_strictly_greater(bi,ball,context0))
	    ball=bi;
	}
	// bound for res and f4buchbergerv
	bound=bres*ball;
	gen bound2=lall*bf4buchberger;
	// lcm of deno and max of coeff
	if (is_strictly_greater(bound2,bound,context0))
	  bound=bound2;
      }
    }
    return true;
  }


  // excluded==-1 and G==identity in calls
  // if eps>0 the check is probabilistic
  template<class tdeg_t,class modint_t>
  bool checkf4buchbergersplit(vectpoly8<tdeg_t> & f4buchbergerv,const vectpoly8<tdeg_t> & res,vector<unsigned> & G,unsigned excluded,double eps){
    if (f4buchbergerv.empty())
      return true;
    polymod<tdeg_t,modint_t> allf4buchberger(f4buchbergerv.front().order,f4buchbergerv.front().dim),rem(allf4buchberger);
    vectpolymod<tdeg_t,modint_t> resmod,quo;
    convert(res,resmod,0);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " checkf4buchberger split begin collect monomials on #polys " << f4buchbergerv.size() << '\n';
    // collect all terms in f4buchbergerv
    collect(f4buchbergerv,allf4buchberger);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " checkf4buchberger split symbolic preprocess" << '\n';
    // find all monomials required to reduce allf4buchberger with res[G[.]]
    polymod<tdeg_t,modint_t> R;
    in_heap_reducemod(allf4buchberger,resmod,G,excluded,quo,rem,&R,0);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " checkf4buchberger split end symbolic preprocess" << '\n';
    // build a matrix with rows res[G[.]]*quo[.] in terms of monomials in allf4buchberger
    // sort the matrix
    // checking reduction to 0 is equivalent to
    // write a line from f4buchbergerv[]
    // as a linear combination of the lines of this matrix
    // we will do that modulo a list of primes
    // and keep track of the coefficients of the linear combination (the quotients)
    // we reconstruct the quotients in Q by fracmod
    // once they stabilize, we compute the lcm l of the denominators
    // we multiply by l to have an equality on Z
    // we compute bounds on the coefficients of the products res*quo
    // and on l*f4buchbergerv, and we check further the equality modulo additional
    // primes until the equality is proved
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin build Mcoeff/Mindex" << '\n';
    vector< vector<gen> > Mcoeff(G.size());
    vector<vector<shifttype> > Mindex;
    vector<coeffindex_t> coeffindex;
    vector<sparse_element> atrier;
    unsigned N=unsigned(R.coord.size()),i,j=0,nterms=0;
    Mindex.reserve(N);
    atrier.reserve(N);
    coeffindex.reserve(N);
    for (i=0;i<G.size();++i){
      Mcoeff[i].reserve(res[G[i]].coord.size());
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      for (;jt!=jtend;++j,++jt){
	Mindex.push_back(vector<shifttype>(0));
#ifdef GIAC_SHORTSHIFTTYPE
	Mindex[j].reserve(1+int(1.1*res[G[i]].coord.size()));
#else
	Mindex[j].reserve(res[G[i]].coord.size());
#endif
      }
    }
    for (i=0,j=0;i<G.size();++i){
      // copy coeffs of res[G[i]] in Mcoeff
      copycoeff(res[G[i]],Mcoeff[i]);
      // for each monomial of quo[i], find indexes and put in Mindex
      typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=quo[i].coord.begin(),jtend=quo[i].coord.end();
      for (;jt!=jtend;++j,++jt){
	coeffindex.push_back(coeffindex_t(N<0xffff,i));
	makelinesplit(res[G[i]],&jt->u,R,Mindex[j]);
	atrier.push_back(sparse_element(first_index(Mindex[j]),j));
      }
    }
    sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1); 
    vector< vector<shifttype> > Mindex1(atrier.size());
    vector<coeffindex_t> coeffindex1(atrier.size());
    for (i=0;i<atrier.size();++i){
      swap(Mindex1[i],Mindex[atrier[i].pos]);
      swap(coeffindex1[i],coeffindex[atrier[i].pos]);
    }
    swap(Mindex,Mindex1);
    swap(coeffindex,coeffindex1);
    // CERR << M << '\n';
    if (debug_infolevel>0)
      CERR << CLOCK()*1e-6 << " rows, columns, terms: " << Mindex.size() << "x" << N << "=" << nterms << '\n'; 
    // PSEUDO_MOD is not interesting here since there is no inter-reduction
    gen p(int(extend(1<<31)-1));
    gen pip(1);
    vectpolymod<tdeg_t,modint_t> f4buchbergervmod;
    matrice coeffmat;
    vector< vector<modint_t> > coeffmatmodp(f4buchbergerv.size(),vector<modint_t>(Mindex.size()));
    gen bres=linfnorm(res,context0);
    gen bf4buchberger=linfnorm(f4buchbergerv,context0);
    matrice prevmatq;
    bool stable=false;
    gen bound=0;
    vector< vector<modint_t> > Mcoeffp(Mcoeff.size());
    for (int iter=0;;++iter){
      if (eps>0 && is_greater(eps*pip,1,context0))
	return true;
      p=prevprime(p-1);
      int env=p.val;
      // check that p does not divide a leading monomial in M
      unsigned j;
      for (j=0;j<Mcoeff.size();++j){
	if (smod(Mcoeff[j].front(),p)==0)
	  break;
      }
      if (j<Mcoeff.size())
	continue;
      // compute Mcoeff mod p
      for (unsigned i=0;i<Mcoeff.size();++i){
	const vector<gen> & Mi=Mcoeff[i];
	vector<modint_t> & Ni=Mcoeffp[i];
	Ni.clear();
	Ni.reserve(Mi.size());
	for (unsigned j=0;j<Mi.size();++j){
	  const gen & Mij=Mi[j];
	  modint_t tmp= Mij.type==_ZINT?modulo(*Mij._ZINTptr,env):Mij.val%env;
	  Ni.push_back(tmp);
	}
      }
      // reduce f4buchbergerv and stores coefficients of quotients for f4buchbergerv[i] in coeffmat[i]
      convert(f4buchbergerv,f4buchbergervmod,env);
      if (debug_infolevel>0)
	CERR << CLOCK()*1e-6 << " checking mod " << p << '\n';
      vector<modint_t> v;
      unsigned countres=0;
#ifdef x86_64
      vector<int128_t> v128;
#endif
      for (unsigned i=0;i<f4buchbergervmod.size();++i){
	makeline(f4buchbergervmod[i],0,R,v);
#ifdef x86_64
	if (!checkreducef4buchbergersplit_64(v,coeffmatmodp[i],Mindex,Mcoeffp,coeffindex,env,v128))
	  return false;
#else
	if (!checkreducef4buchbergersplit(v,coeffmatmodp[i],Mindex,Mcoeffp,coeffindex,env))
	  return false;
#endif
	if (iter==0){
	  unsigned countrescur=0;
	  vector<modint_t> & coeffi=coeffmatmodp[i];
	  for (unsigned j=0;j<coeffi.size();++j){
	    if (coeffi[j])
	      ++countrescur;
	  }
	  if (countrescur>countres)
	    countres=countrescur;
	}
      }
      // if (iter==0) bound=pow(bres,int(countres),context0);
      if (stable){
	if (!chk_equal_mod(prevmatq,coeffmatmodp,env))
	  stable=false;
	// if stable compute bounds and compare with product of primes
	// if 2*bounds < product of primes recheck stabilization and return true
	if (is_strictly_greater(pip,bound,context0)){
	  if (debug_infolevel>0)
	    CERR << CLOCK()*1e-6 << " modular check finished " << '\n';
	  return true;
	}
      }
      // combine coeffmat with previous one by chinese remaindering
      if (debug_infolevel>0)
	CERR << CLOCK()*1e-6 << " chinrem mod " << p << '\n';
      if (iter)
	ichinrem_inplace(coeffmat,coeffmatmodp,pip,p.val);
      else
	vectvector_int2vecteur(coeffmatmodp,coeffmat);
      pip=pip*p;
      if (is_greater(bound,pip,context0))
	continue;
      if (!stable){
	// check stabilization 
	matrice checkquo;
	checkquo.reserve(coeffmat.size());
	for (unsigned k=0;k<coeffmat.size();++k){
	  if (prevmatq.size()>k && chk_equal_mod(prevmatq[k],coeffmatmodp[k],env))
	    checkquo.push_back(prevmatq[k]);
	  else
	    checkquo.push_back(fracmod(coeffmat[k],pip));
	  if (prevmatq.size()>k && checkquo[k]!=prevmatq[k])
	    break;
	  if (k>(prevmatq.size()*3)/2+2)
	    break;
	}
	if (checkquo!=prevmatq){
	  swap(prevmatq,checkquo);
	  if (debug_infolevel>0)
	    CERR << CLOCK()*1e-6 << " unstable mod " << p << " reconstructed " << prevmatq.size() << '\n';
	  continue;
	}
	matrice coeffmatq=*_copy(checkquo,context0)._VECTptr;
	if (debug_infolevel>0)
	  CERR << CLOCK()*1e-6 << " full stable mod " << p << '\n';
	stable=true;
	gen lall=1; vecteur l(coeffmatq.size());
	for (unsigned i=0;i<coeffmatq.size();++i){
	  lcmdeno(*coeffmatq[i]._VECTptr,l[i],context0);
	  if (is_strictly_greater(l[i],lall,context0))
	    lall=l[i];
	}
	if (debug_infolevel>0)
	  CERR << CLOCK()*1e-6 << " lcmdeno ok/start bound " << p << '\n';
	gen ball=1,bi; // ball is the max bound of all coeff in coeffmatq
	for (unsigned i=0;i<coeffmatq.size();++i){
	  bi=linfnorm(coeffmatq[i],context0);
	  if (is_strictly_greater(bi,ball,context0))
	    ball=bi;
	}
	// bound for res and f4buchbergerv
	bound=bres*ball;
	gen bound2=lall*bf4buchberger;
	// lcm of deno and max of coeff
	if (is_strictly_greater(bound2,bound,context0))
	  bound=bound2;
      }
    }
    return true;
  }

  /* ***************
     BEGIN ZPOLYMOD
     ***************  */
#if GIAC_SHORTSHIFTTYPE==16
  // Same algorithms compressing data
  // since all spolys reduced at the same time share the same exponents
  // we will keep the exponents only once in memory
  // sizeof(zmodint)=8 bytes, sizeof(T_unsigned<modint,tdeg_t>)=28 or 36
  // typedef T_unsigned<modint,unsigned> zmodint;
  template <class tdeg_t,class modint_t>
  struct zpolymod {
    order_t order;
    short int dim;
    bool in_gbasis; // set to false in zgbasis_updatemod for "small" reductors that we still want to use for reduction
    short int age:15;
    vector< T_unsigned<modint_t,unsigned> > coord;
    const vector<tdeg_t> * expo;
    tdeg_t ldeg;
    int maxtdeg;
    int fromleft,fromright;
    double logz;
    zpolymod():in_gbasis(true),dim(0),expo(0),ldeg(),age(0),fromleft(-1),fromright(-1),logz(1) {order.o=0; order.lex=0; order.dim=0; maxtdeg=-1;}
    zpolymod(order_t o,int d): in_gbasis(true),dim(d),expo(0),ldeg(),age(0),fromleft(-1),fromright(-1),logz(1) {order=o; order.dim=d; maxtdeg=-1;}
    zpolymod(order_t o,int d,const tdeg_t & l): in_gbasis(true),dim(d),expo(0),ldeg(l),age(0),fromleft(-1),fromright(-1),logz(1) {order=o; order.dim=d; maxtdeg=-1;}
    zpolymod(order_t o,int d,const vector<tdeg_t> * e,const tdeg_t & l): in_gbasis(true),dim(d),expo(e),ldeg(l),age(0),fromleft(-1),fromright(-1),logz(1) {order=o; order.dim=d; maxtdeg=-1;}
    void dbgprint() const;
    void compute_maxtdeg(){
      if (expo){
	typename std::vector<  T_unsigned<modint_t,unsigned>  >::iterator pt=coord.begin(),ptend=coord.end();
	for (;pt!=ptend;++pt){
	  int tmp=(*expo)[pt->u].total_degree(order);
	  if (tmp>maxtdeg)
	    maxtdeg=tmp;
	}
      }
    }
  };

  template<class tdeg_t>
  struct zinfo_t {
    vector< vector<tdeg_t> > quo;
    vector<tdeg_t> R,rem;
    vector<int> permu;
    vector< paire > B;
    vector<unsigned> G,permuB;
    unsigned nonzero,Ksizes;
  };

  template<class tdeg_t,class modint_t>
  void zsmallmultmod(modint_t a,zpolymod<tdeg_t,modint_t> & p,modint_t m){
    typename std::vector<  T_unsigned<modint_t,unsigned>  >::iterator pt=p.coord.begin(),ptend=p.coord.end();
#if 1 // ndef GBASIS_4PRIMES
    if (a==1 || a==create<modint_t>(1)-m){
      for (;pt!=ptend;++pt){
	modint_t tmp=pt->g;
	pt->g=makepositive(tmp,m);
        // if (tmp<0) tmp += m; pt->g=tmp;
      }
      return;
    }
#endif
    for (;pt!=ptend;++pt){
      modint_t tmp=(extend(pt->g)*a)%m;
      pt->g=makepositive(tmp,m); // if (tmp<0) tmp += m;       pt->g=tmp;
    }
  }

  template<class tdeg_t,class modint_t>
  bool operator == (const zpolymod<tdeg_t,modint_t> & p,const zpolymod<tdeg_t,modint_t> &q){
    if (p.coord.size()!=q.coord.size() || p.expo!=q.expo)
      return false;
    for (unsigned i=0;i<p.coord.size();++i){
      if (p.coord[i].u!=q.coord[i].u || p.coord[i].g!=q.coord[i].g)
	return false;
    }
    return true;
  }

#ifdef NSPIRE
  template<class T,class tdeg_t,class modint_t>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os, const zpolymod<tdeg_t,modint_t> & p)
#else
  template<class tdeg_t,class modint_t>
  ostream & operator << (ostream & os, const zpolymod<tdeg_t,modint_t> & p)
#endif
  {
    if (!p.expo)
      return os << "error, null pointer in expo " ;
    typename std::vector< T_unsigned<modint_t,unsigned> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    int t2;
    os << "zpolymod(" << p.logz << "," << p.age << ":" << p.fromleft << "," << p.fromright << "): ";
    if (it==itend)
      return os << 0 ;
    for (;it!=itend;){
      os << it->g  ;
#ifndef GBASIS_NO_OUTPUT
      if ((*p.expo)[it->u].vars64()){
	if ((*p.expo)[it->u].tdeg%2){
	  degtype * i=(degtype *)((*p.expo)[it->u].ui+1);
	  for (int j=0;j<(*p.expo)[it->u].order_.dim;++j){
	    t2=i[j];
	    if (t2)
	      os << "*x"<< j << "^" << t2  ;
	  }
	  ++it;
	  if (it==itend)
	    break;
	  os << " + ";
	  continue;
	}
      }
#endif
      short tab[GROEBNER_VARS+1];
      (*p.expo)[it->u].get_tab(tab,p.order);
      switch (p.order.o){
      case _PLEX_ORDER:
	for (int i=0;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2)
	    os << "*x"<< i << "^" << t2  ;
	}
	break;
      case _TDEG_ORDER:
	for (int i=1;i<=GROEBNER_VARS;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  if (t2)
	    os << "*x"<< i-1 << "^" << t2  ;
	}
	break;
      case _REVLEX_ORDER:
	for (int i=1;i<=GROEBNER_VARS && i<=p.dim;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;
#if GROEBNER_VARS==15
      case _3VAR_ORDER:
	for (int i=1;i<=3;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 3-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=5;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 7+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
      case _7VAR_ORDER:
	for (int i=1;i<=7;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 7-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=9;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 11+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
      case _11VAR_ORDER:
	for (int i=1;i<=11;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 11-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	for (int i=13;i<=15;++i){
	  t2 = tab[i];
	  if (t2==0)
	    continue;
	  os << "*x"<< 15+p.dim-i;
	  if (t2!=1)
	    os << "^" << t2;
	}
	break;	
#endif
      }
      ++it;
      if (it==itend)
	break;
      os << " + ";
    }
    return os;
  }

  template<class tdeg_t,class modint_t>
  void zpolymod<tdeg_t,modint_t>::dbgprint() const { 
    CERR << *this << '\n';
  }

  template<class tdeg_t,class modint_t>
  class vectzpolymod:public vector< zpolymod<tdeg_t,modint_t> >{
  public:
    void dbgprint() const { CERR << *this << '\n'; }
  };

  template<class tdeg_t,class modint_t>
  void zleftright(const vectzpolymod<tdeg_t,modint_t> & res,const vector< paire > & B,vector<tdeg_t> & leftshift,vector<tdeg_t> & rightshift){
    tdeg_t l;
    for (unsigned i=0;i<B.size();++i){
      const zpolymod<tdeg_t,modint_t> & p=res[B[i].first];
      const zpolymod<tdeg_t,modint_t> & q=res[B[i].second];
      if (debug_infolevel>2)
	CERR << "zleftright " << p << "," << q << '\n';
      index_lcm_overwrite(p.ldeg,q.ldeg,l,p.order);
      leftshift[i]=l-p.ldeg;
      rightshift[i]=l-q.ldeg;
    }
  }

  // collect monomials from pairs of res (vector of polymod<tdeg_t,modint_t>s), shifted by lcm
  // does not collect leading monomial (since they cancel)
  template<class tdeg_t,class modint_t>
  bool zcollect(const vectzpolymod<tdeg_t,modint_t> & res,const vector< paire > & B,const vector<unsigned> & permuB,vector<tdeg_t> & allf4buchberger,vector<tdeg_t> & leftshift,vector<tdeg_t> & rightshift){
    int start=1,countdiscarded=0;
    vector<heap_tt<tdeg_t> > Ht;
    heap_tt<tdeg_t> heap_elem;
    vector<heap_tt_ptr<tdeg_t> > H; 
    Ht.reserve(2*B.size()+1);
    H.reserve(2*B.size());
    unsigned s=0;
    order_t keyorder={_REVLEX_ORDER,0};
    for (unsigned i=0;i<B.size();++i){
      unsigned truei=permuB[i];
      const paire & Bi=B[truei];
      const zpolymod<tdeg_t,modint_t> & p=res[Bi.first];
      const zpolymod<tdeg_t,modint_t> & q=res[Bi.second];
      keyorder=p.order;
      bool eq=i>0 && Bi.second==B[permuB[i-1]].second && rightshift[truei]==rightshift[permuB[i-1]];
      if (int(p.coord.size())>start){
	s = giacmax(s, unsigned(p.coord.size()));
	Ht.push_back(heap_tt<tdeg_t>(true,truei,start,(*p.expo)[p.coord[start].u]+leftshift[truei]));
	H.push_back(heap_tt_ptr<tdeg_t>(&Ht.back()));
      }
      if (!eq && int(q.coord.size())>start){
	s = giacmax(s, unsigned(q.coord.size()));
	Ht.push_back(heap_tt<tdeg_t>(false,truei,start,(*q.expo)[q.coord[start].u]+rightshift[truei]));
	H.push_back(heap_tt_ptr<tdeg_t>(&Ht.back()));
      }
    }
    allf4buchberger.reserve(s); // int(s*std::log(1+H.size())));
    compare_heap_tt_ptr<tdeg_t> key(keyorder);
    make_heap(H.begin(),H.end(),key);
    while (!H.empty()){
      // push root node of the heap in allf4buchberger
      heap_tt<tdeg_t> & current = *H.front().ptr;
      if (int(current.u.total_degree(keyorder))>GBASISF4_MAX_TOTALDEG){
	CERR << "Error zcollect total degree too large" << current.u.total_degree(keyorder) << '\n';
	return false;
      }
      if (allf4buchberger.empty() || allf4buchberger.back()!=current.u)
	allf4buchberger.push_back(current.u);
      unsigned vpos;
      if (current.left)
	vpos=B[current.f4buchbergervpos].first;
      else
	vpos=B[current.f4buchbergervpos].second;
      ++current.polymodpos;
      const zpolymod<tdeg_t,modint_t> & resvpos=res[vpos];
      for (int startheappos=1;current.polymodpos<resvpos.coord.size();++current.polymodpos){
	add((*resvpos.expo)[resvpos.coord[current.polymodpos].u],current.left?leftshift[current.f4buchbergervpos]:rightshift[current.f4buchbergervpos],current.u,keyorder.dim);
	// if (current.left) current.u=(*resvpos.expo)[resvpos.coord[current.polymodpos].u]+leftshift[current.f4buchbergervpos]; else current.u=(*resvpos.expo)[resvpos.coord[current.polymodpos].u]+rightshift[current.f4buchbergervpos];
	int newtdeg=current.u.total_degree(keyorder);
	// quick look in part of heap: if monomial is already there, increment current.polymodpos
	int heappos=startheappos,ntests=int(H.size()); // giacmin(8,H.size());
	for (;heappos<ntests;){
	  heap_tt<tdeg_t> & heapcurrent=*H[heappos].ptr;
	  int heapcurtdeg=heapcurrent.u.total_degree(keyorder);
	  if (heapcurtdeg<newtdeg){
	    heappos=ntests; break;
	  }
	  if (heapcurtdeg==newtdeg && heapcurrent.u==current.u)
	    break;
	  //if (heappos<8) ++heappos; else
	    heappos=2*heappos+1;
	}
	if (heappos<ntests){
	  ++countdiscarded;
	  startheappos=2*heappos+1;
	  continue;
	}
	break;
      }
      // push_back &current into heap so that pop_heap will bubble out the
      // modified root node (initialization will exchange two identical pointers)
      if (current.polymodpos<resvpos.coord.size()){
	H.push_back(heap_tt_ptr<tdeg_t>(&current));
        std::push_heap(H.begin(),H.end(),key); // ?clang
      }
      std::pop_heap(H.begin(),H.end(),key);
      H.pop_back();
    }
    if (debug_infolevel>1)
      CERR << "pairs " << B.size() << ", discarded monomials " << countdiscarded << '\n';
    return true;
  }

  // returns heap actual size, 0 means no quotients (all elements of q empty())
template<class tdeg_t,class modint_t>
  size_t zsymbolic_preprocess(const vector<tdeg_t> & f,const vectzpolymod<tdeg_t,modint_t> & g,const vector<unsigned> & G,unsigned excluded,vector< vector<tdeg_t> > & q,vector<tdeg_t> & rem,vector<tdeg_t> & R){
    int countdiscarded=0;
    // divides f by g[G[0]] to g[G[G.size()-1]] except maybe g[G[excluded]]
    // CERR << f << "/" << g << '\n';
    // first implementation: use quotient heap for all quotient/divisor
    // do not use heap chain
    // ref Monaghan Pearce if g.size()==1
    // R is the list of all monomials
    if (f.empty() || G.empty())
      return 0;
    int dim=g[G.front()].dim;
    order_t order=g[G.front()].order;
#ifdef GIAC_GBASIS_PERMUTATION1
    // First reorder G in order to use the "best" possible reductor
    // This is done using the ldegree of g[G[i]] (should be minmal)
    // and the number of terms (should be minimal)
    vector<zsymb_data<tdeg_t> > GG(G.size());
    for (unsigned i=0;i<G.size();++i){
      zsymb_data<tdeg_t> zz={i,g[G[i]].ldeg,g[G[i]].order,unsigned(g[G[i]].coord.size()),g[G[i]].age,0.0};
      GG[i]=zz;
    }
    sort(GG.begin(),GG.end());
#endif
    tdeg_t minldeg(g[G.front()].ldeg);
    R.clear();
    rem.clear();
    // if (G.size()>q.size()) q.clear();
    q.resize(G.size());
    unsigned guess=0;
    for (unsigned i=0;i<G.size();++i){
      q[i].clear();
      guess += unsigned(g[G[i]].coord.size());
      if (!tdeg_t_greater(g[G[i]].ldeg,minldeg,order))
	minldeg=g[G[i]].ldeg;
    }
    vector<heap_t<tdeg_t> > H_;
    vector<unsigned> H;
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " Heap reserve " << 3*f.size() << " * " << sizeof(heap_t<deg_t>)+sizeof(unsigned) << ", number of monomials in basis " << guess  << '\n';
    // H_.reserve(guess);
    // H.reserve(guess);
    H_.reserve(3*f.size());
    H.reserve(3*f.size());
    heap_t_compare<tdeg_t> key(H_,order);
    unsigned k=0,i; // k=position in f
    tdeg_t m;
    bool finish=false;
    while (!H.empty() || k<f.size()){
      // is highest remaining degree in f or heap?
      if (k<f.size() && (H.empty() || tdeg_t_greater(f[k],H_[H.front()].u,order)) ){
	// it's in f or both
	m=f[k];
	++k;
      }
      else {
	m=H_[H.front()].u;
      }
      //CERR << m << '\n';
      R.push_back(m);
      // extract from heap all terms having m as monomials, subtract from c
      while (!H.empty() && H_[H.front()].u==m){
	heap_t<tdeg_t> & current=H_[H.front()]; // was root node of the heap
	const zpolymod<tdeg_t,modint_t> & gcurrent = g[G[current.i]];
	++current.gj;
	for (int startheappos=1;current.gj<gcurrent.coord.size();++current.gj){
	  //current.u=q[current.i][current.qi]+(*gcurrent.expo)[gcurrent.coord[current.gj].u];
	  add(q[current.i][current.qi],(*gcurrent.expo)[gcurrent.coord[current.gj].u],current.u,dim);
	  int newtdeg=current.u.total_degree(order);
	  // quick look in part of heap: is monomial already there?
	  int heappos=startheappos,ntests=int(H.size()); // giacmin(8,H.size());
	  for (;heappos<ntests;){
	    heap_t<tdeg_t> & heapcurrent=H_[H[heappos]];
	    int heapcurtdeg=heapcurrent.u.total_degree(order);
	    if (heapcurtdeg<newtdeg){
	      heappos=ntests; break;
	    }
	    if (heapcurtdeg==newtdeg && heapcurrent.u==current.u)
	      break;
	    //if (heappos<8) ++heappos; else
	      heappos=2*heappos+1;
	  }
	  if (heappos<ntests){
	    ++countdiscarded;
	    startheappos=2*heappos+1;
	    continue;
	  }
	  break;
	}
	if (current.gj<gcurrent.coord.size()){
	  H.push_back(H.front());
          std::push_heap(H.begin(),H.end(),key); // ?clang
          std::pop_heap(H.begin(),H.end(),key);
	  H.pop_back();
	}
	else {
	  std::pop_heap(H.begin(),H.end(),key);
	  H.pop_back();
	}
      } // end while !H.empty()
      // divide (c,m) by one of the g if possible, otherwise push in remainder
      if (finish){
	rem.push_back(m); // add to remainder
	continue;
      }
#ifdef GIAC_DEG_FIRST
      int mtot=m.total_degree(order);
#endif
      unsigned ii;
      for (ii=0;ii<G.size();++ii){
#ifdef GIAC_GBASIS_PERMUTATION1
	i=GG[ii].pos; // we can use any permutation of 0..G.size()-1 here
#else
	i=ii; 
#endif
	if (i==excluded)
	  continue;
	const tdeg_t & deg=g[G[i]].ldeg;
#ifdef GIAC_DEG_FIRST
	if (deg.total_degree(order)>mtot){
	  ii=G.size(); break;
	}
#endif
	if (tdeg_t_all_greater(m,deg,order))
	  break;
      }
      if (ii==G.size()){
	rem.push_back(m); // add to remainder
	// no monomial divide m, check if m is greater than one of the monomial of G
	// if not we can push all remaining monomials in rem
	finish=!tdeg_t_greater(m,minldeg,order);
	continue;
      }
      // add m/leading monomial of g[G[i]] to q[i]
      const zpolymod<tdeg_t,modint_t> & gGi=g[G[i]];
      tdeg_t monom=m-gGi.ldeg;
      q[i].push_back(monom);
      // CERR << i << " " << q[i] << '\n';
      // push in heap
      int startheappos=0;
      for (int pos=1;pos<gGi.coord.size();++pos){
#if 0 // not useful here, but would be above!
	tdeg_t newmonom=(*gGi.expo)[gGi.coord[pos].u] + monom;
	int newtdeg=newmonom.total_degree(order);
	// quick look in part of heap is monomial already there
	int heappos=startheappos,ntests=H.size(); // giacmin(8,H.size());
	for (;heappos<ntests;heappos=2*heappos+1){
	  heap_t<tdeg_t> & current=H_[H[heappos]];
	  int curtdeg=current.u.total_degree(order);
	  if (curtdeg<newtdeg){
	    heappos=ntests; break;
	  }
	  if (curtdeg==newtdeg && current.u==newmonom)
	    break;
	}
	if (heappos<ntests){
	  ++countdiscarded;
	  startheappos=2*heappos+1;
	  continue;
	}
	heap_t<tdeg_t> current = { i, unsigned(q[i].size()) - 1, pos, newmonom };
#else
	heap_t<tdeg_t> current = { i, unsigned(q[i].size()) - 1, unsigned(pos), (*gGi.expo)[gGi.coord[pos].u] + monom };
#endif
	H.push_back(hashgcd_U(H_.size()));
	H_.push_back(current);
	key.ptr=&H_.front();
	std::push_heap(H.begin(),H.end(),key);
	break;
      }
    } // end main heap pseudo-division loop
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " Heap actual size was " << H_.size() << " discarded monomials " << countdiscarded << '\n';
    return H_.size();
  }

  template<class tdeg_t,class modint_t>
  void zcopycoeff(const zpolymod<tdeg_t,modint_t> & p,vector<modint_t> & v,modint_t env,int start){
    typename std::vector<  T_unsigned<modint_t,unsigned>  >::const_iterator it=p.coord.begin()+start,itend=p.coord.end();
    v.clear();
    v.reserve(itend-it);
    for (;it!=itend;++it){
      modint_t g=it->g;
      if (g<0) g += env;
      v.push_back(g);
    }
  }

  template<class tdeg_t,class modint_t>
  void zcopycoeff(const zpolymod<tdeg_t,modint_t> & p,vector<modint_t> & v,int start){
    typename std::vector<  T_unsigned<modint_t,unsigned>  >::const_iterator it=p.coord.begin()+start,itend=p.coord.end();
    v.clear();
    v.reserve(itend-it);
    for (;it!=itend;++it){
      modint_t g=it->g;
      v.push_back(g);
    }
  }

  // dichotomic seach for jt->u==u in [jt,jtend[
  template<class tdeg_t>
  bool dicho(typename std::vector<tdeg_t>::const_iterator & jt,typename std::vector<tdeg_t>::const_iterator jtend,const tdeg_t & u,order_t order){
    if (*jt==u) return true;
    if (jtend-jt<=6){ ++jt; return false; }// == test faster
    for (;;){
      int step=int((jtend-jt)/2);
      typename std::vector<tdeg_t>::const_iterator j=jt+step;
      if (j==jt)
	return *j==u;
      //PREFETCH(&*(j+step/2));
      //PREFETCH(&*(jt+step/2));
      if (int res=tdeg_t_greater(*j,u,order)){
	jt=j;
	if (res==2)
	  return true;
      }
      else
	jtend=j;
    }
  }

#if 1 // #if 0 for old versions of gcc
  template<>
  bool dicho(std::vector<tdeg_t64>::const_iterator & jt,std::vector<tdeg_t64>::const_iterator jtend,const tdeg_t64 & u,order_t order){
    if (*jt==u) return true;
    if (jtend-jt<=6){ ++jt; return false; }// == test faster
#ifdef GIAC_ELIM
    if (u.tab[0]%2){
      int utdeg=u.tab[0],utdeg2=u.tdeg2;
      ulonglong uelim=u.elim;
      for (;;){
	int step=(jtend-jt)/2;
	std::vector<tdeg_t64>::const_iterator j=jt+step;
	if (j==jt)
	  return *j==u;
	if (j->tab[0]!=utdeg){
	  if (j->tdeg>utdeg)
	    jt=j;
	  else
	    jtend=j;
	  continue;
	}
	if (j->tdeg2!=utdeg2){
	  if (j->tdeg2>utdeg2)
	    jt=j;
	  else
	    jtend=j;
	  continue;
	}
	if (j->elim!=uelim){
	  if (j->elim<uelim)
	    jt=j;
	  else
	    jtend=j;
	  continue;
	}
	if (int res=tdeg_t_greater(*j,u,order)){
	  jt=j;
	  if (res==2)
	    return true;
	}
	else
	  jtend=j;
      }
    }
#endif
    for (;;){
      int step=int((jtend-jt)/2);
      std::vector<tdeg_t64>::const_iterator j=jt+step;
      if (j==jt)
	return *j==u;
      if (int res=tdeg_t_greater(*j,u,order)){
	jt=j;
	if (res==2)
	  return true;
      }
      else
	jtend=j;
    }
  }
#endif

  template<class tdeg_t,class modint_t>
  void zmakelinesplit(const zpolymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const vector<tdeg_t> & R,void * Rhashptr,const vector<int> & Rdegpos,vector<shifttype> & v,vector<shifttype> * prevline,int start=0){
    typename std::vector< T_unsigned<modint_t,unsigned> >::const_iterator it=p.coord.begin()+start,itend=p.coord.end();
    typename std::vector<tdeg_t>::const_iterator Rbegin=R.begin(),jt=Rbegin,jtend=R.end();
    double nop1=double(R.size()); 
    double nop2=2*p.coord.size()*std::log(nop1)/std::log(2.0);
    bool dodicho=nop2<nop1;
    const vector<tdeg_t> & expo=*p.expo;
    unsigned pos=0,Rpos=0;
    if (shiftptr){
      tdeg_t u=*shiftptr+*shiftptr; // create a new memory slot
      const shifttype * st=prevline?&prevline->front():0;
      for (;it!=itend;++it){
	add(expo[it->u],*shiftptr,u,p.dim);
#ifdef GIAC_RHASH
	int hashi=u.hash_index(Rhashptr);
	if (hashi>=0){
	  pushsplit(v,pos,hashi);
	  ++jt;
	  continue;
	}
#endif
	if (dodicho){ 
	  typename std::vector<tdeg_t>::const_iterator end=jtend;
	  if (st){
	    next_index(Rpos,st);
	    end=Rbegin+Rpos;
	  }
#ifdef GIAC_RDEG
	  int a=Rdegpos[u.tdeg+1],b=Rdegpos[u.tdeg];
	  if (jt-Rbegin<a)
	    jt=Rbegin+a;
	  if (end-Rbegin>b)
	    end=Rbegin+b;
#endif
	  if (dicho(jt,end,u,p.order)){
	    pushsplit(v,pos,unsigned(jt-Rbegin));
	    ++jt;
	    continue;
	  }
	}
	for (;jt!=jtend;++jt){
	  if (*jt==u){
	    pushsplit(v,pos,int(jt-Rbegin));
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=expo[it->u];
#ifdef GIAC_RHASH
	int hashi=u.hash_index(Rhashptr);
	if (hashi>=0){
	  pushsplit(v,pos,hashi);
	  ++jt;
	  continue;
	}
#endif
#if 1
	if (dodicho && dicho(jt,jtend,u,p.order)){
	  pushsplit(v,pos,unsigned(jt-Rbegin));
	  ++jt;
	  continue;
	}
#endif
	for (;jt!=jtend;++jt){
	  if (*jt==u){
	    pushsplit(v,pos,int(jt-Rbegin));
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void zmakeline(const zpolymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const vector<tdeg_t> & R,vector<modint_t> & v,int start=0){
    int Rs=int(R.size());
    // if (v.size()!=Rs) v.resize(Rs); 
    // v.assign(Rs,0);
    typename std::vector< T_unsigned<modint_t,unsigned> >::const_iterator it=p.coord.begin()+start,itend=p.coord.end();
    typename std::vector<tdeg_t>::const_iterator jt=R.begin(),jtbeg=jt,jtend=R.end();
    double nop1=double(R.size()); 
    double nop2=2*p.coord.size()*std::log(nop1)/std::log(2.0);
    bool dodicho=nop2<nop1;
    const std::vector<tdeg_t> & expo=*p.expo;
    if (shiftptr){
      tdeg_t u=R.front()+R.front(); // create u with refcount 1
      for (;it!=itend;++it){
	add(expo[it->u],*shiftptr,u,p.dim);
	if (dodicho && dicho(jt,jtend,u,p.order)){
	  v[jt-jtbeg]=it->g;
	  ++jt;
	  continue;
	}	
	for (;jt!=jtend;++jt){
	  if (*jt==u){
	    v[jt-jtbeg]=it->g;
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=expo[it->u];
	if (dodicho && dicho(jt,jtend,u,p.order)){
	  v[jt-jtbeg]=it->g;
	  ++jt;
	  continue;
	}	
	for (;jt!=jtend;++jt){
	  if (*jt==u){
	    v[jt-jtbeg]=it->g;
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void zmakelinesub(const zpolymod<tdeg_t,modint_t> & p,const tdeg_t * leftshift,const zpolymod<tdeg_t,modint_t> & q,const tdeg_t * rightshift,const vector<tdeg_t> & R,vector<modint_t> & v,int start,modint_t env){
    typename std::vector<  T_unsigned<modint_t,unsigned>  >::const_iterator pt=p.coord.begin()+start,ptend=p.coord.end();
    typename std::vector<  T_unsigned<modint_t,unsigned>  >::const_iterator qt=q.coord.begin()+start,qtend=q.coord.end();
    typename std::vector<tdeg_t>::const_iterator jt=R.begin(),jtbeg=jt,jtend=R.end();
    const std::vector<tdeg_t> & pexpo=*p.expo;
    const std::vector<tdeg_t> & qexpo=*q.expo;
    double nop1=double(R.size()); 
    double nop2=2*p.coord.size()*std::log(nop1)/std::log(2.0);
    bool dodicho=nop2<nop1;
    int dim=p.dim;
    order_t order=p.order;
    if (leftshift && rightshift){
      tdeg_t ul=R.front()+R.front();
      tdeg_t ur=R.front()+R.front();
      bool updatep=true,updateq=true;
      for (;pt!=ptend && qt!=qtend;){
	if (updatep){
	  add(pexpo[pt->u],*leftshift,ul,dim);
	  updatep=false;
	}
	if (updateq){
	  add(qexpo[qt->u],*rightshift,ur,dim);
	  updateq=false;
	}
	if (tdeg_t_greater(ul,ur,order)){
	  if (dodicho && dicho(jt,jtend,ul,order)){
	    if (ul==ur){
	      v[jt-jtbeg] = (pt->g-extend(qt->g)); // %env;
	      ++jt; ++pt; ++qt;
	      updatep=updateq=true;
	      continue;
	    }
	    v[jt-jtbeg] = pt->g;
	    ++jt; ++pt;
	    updatep=true;
	    continue;
	  }
	  for (;jt!=jtend;++jt){
	    if (*jt==ul){
	      if (ul==ur){
		v[jt-jtbeg] = (pt->g-extend(qt->g)); // %env;
		++jt; ++pt; ++qt;
		updatep=updateq=true;
	      }
	      else {
		v[jt-jtbeg] = pt->g;
		++jt; ++pt;
		updatep=true;
	      }
	      break;
	    }
	  }
	  continue;
	} // end if ul>=ur
	if (dodicho && dicho(jt,jtend,ur,order)){
	  v[jt-jtbeg] = -qt->g;
	  ++jt; ++qt;
	  updateq=true;
	  continue;
	}
	for (;jt!=jtend;++jt){
	  if (*jt==ur){
	    v[jt-jtbeg] = -qt->g;
	    ++jt; ++qt;
	    updateq=true;
	    break;
	  }
	}
      } // for (pt!=ptend && qt!=qtend)
      for (;pt!=ptend;){
	add(pexpo[pt->u],*leftshift,ul,dim);
	if (dodicho && dicho(jt,jtend,ul,order)){
	  v[jt-jtbeg] = pt->g;
	  ++jt; ++pt;
	  continue;
	}
	for (;jt!=jtend;++jt){
	  if (*jt==ul){
	    v[jt-jtbeg] = pt->g;
	    ++jt; ++pt;
	    break;
	  }
	}
      } 
      for (;qt!=qtend;){
	add(qexpo[qt->u],*rightshift,ur,dim);
	if (dodicho && dicho(jt,jtend,ur,order)){
	  v[jt-jtbeg] = -qt->g;
	  ++jt; ++qt;
	  continue;
	}
	for (;jt!=jtend;++jt){
	  if (*jt==ur){
	    v[jt-jtbeg] = -qt->g;
	    ++jt; ++qt;
	    break;
	  }
	}
      } 
    }
  }

  template<class tdeg_t,class modint_t>
  void zmakelinesub(const zpolymod<tdeg_t,modint_t> & p,const tdeg_t * shiftptr,const vector<tdeg_t> & R,vector<modint_t> & v,int start,modint_t env){
    typename std::vector<  T_unsigned<modint_t,unsigned>  >::const_iterator it=p.coord.begin()+start,itend=p.coord.end();
    typename std::vector<tdeg_t>::const_iterator jt=R.begin(),jtbeg=jt,jtend=R.end();
    const std::vector<tdeg_t> & expo=*p.expo;
    double nop1=double(R.size()); 
    double nop2=2*p.coord.size()*std::log(nop1)/std::log(2.0);
    bool dodicho=nop2<nop1;
    if (shiftptr){
      tdeg_t u=R.front()+R.front();
      for (;it!=itend;++it){
	add(expo[it->u],*shiftptr,u,p.dim);
	if (dodicho && dicho(jt,jtend,u,p.order)){
#if 1
	  v[jt-jtbeg] -= it->g;
#else
	  modint_t & vv=v[jt-jtbeg];
	  if (vv)
	    vv = (vv-extend(it->g))%env;
	  else
	    vv=-it->g;
#endif
	  ++jt;
	  continue;
	}
	for (;jt!=jtend;++jt){
	  if (*jt==u){
#if 1
	    v[jt-jtbeg] -= it->g;
#else
	    modint_t & vv=v[jt-jtbeg];
	    if (vv)
	      vv = (vv-extend(it->g));//%env;
	    else
	      vv = -it->g;
#endif
	    ++jt;
	    break;
	  }
	}
      }
    }
    else {
      for (;it!=itend;++it){
	const tdeg_t & u=expo[it->u];
	if (dodicho && dicho(jt,jtend,u,p.order)){
#if 1
	  v[jt-jtbeg] -= it->g;
#else
	  modint_t & vv=v[jt-jtbeg];
	  vv = (vv-extend(it->g))%env;
#endif
	  ++jt;
	  continue;
	}
	for (;jt!=jtend;++jt){
	  if (*jt==u){
#if 1
	    v[jt-jtbeg]-=it->g;
#else
	    modint_t & vv=v[jt-jtbeg];
	    vv = (vv-extend(it->g))%env;
#endif
	    ++jt;
	    break;
	  }
	}
      }
    }
  }

  template<class modint_t,class modint_t2>
  void zsub(vector<modint_t2> & v64,const vector<modint_t> & subcoeff,const vector<shifttype> & subindex){
    if (subcoeff.empty()) return;
    typename vector<modint_t2>::iterator wt=v64.begin();
    const modint_t * jt=&subcoeff.front(),*jtend=jt+subcoeff.size(),*jt_=jtend-8;
    const shifttype * it=&subindex.front();
    // first shift
    unsigned pos=0; next_index(pos,it); wt += pos;
    *wt -= (*jt); ++jt;
    bool shortshifts=v64.size()<0xffff?true:checkshortshifts(subindex);
#ifdef GIAC_SHORTSHIFTTYPE
    if (shortshifts){
#if GIAC_SHORTSHIFTTYPE==16 && !defined BIGENDIAN
      if ( jt<jtend && ((ulonglong) it) & 0x2){ // align it to 32 bits
	wt += *it; ++it;
	*wt -= (*jt); ++jt;
      }
      jtend-=2;
      for (;jt<=jtend;){
        unsigned * IT=(unsigned *) it;
	wt += ((*IT) & 0xffff);
	*wt -= (*jt); ++jt;
	wt += ((*IT) >>16);
	*wt -= (*jt); ++jt;
        it+=2;
      }
      jtend+=2;
#endif
      for (;jt!=jtend;++jt){
	wt += *it; ++it;
	*wt -= (*jt);
      }
    }
    else {
      for (;jt!=jtend;++jt){
	next_index(wt,it);
	*wt -= (*jt);
      }
    }
#else // def GIAC_SHORTSHIFTTYPE
    for (;jt!=jtend;++jt){
      v64[*it] -= (*jt);
      ++it; ++jt;
    }
#endif // def GIAC_SHORTSHIFTTYPE
  }
    
  template<class tdeg_t,class modint_t,class modint_t2>
  void zadd(vector<modint_t2> & v64,const zpolymod<tdeg_t,modint_t> & subcoeff,const vector<shifttype> & subindex,int start,modint_t env){
    if (subcoeff.coord.size()<=start) return;
    typename vector<modint_t2>::iterator wt=v64.begin();
    const  T_unsigned<modint_t,unsigned>  * jt=&subcoeff.coord.front(),*jtend=jt+subcoeff.coord.size();
    jt += start;
    const shifttype * it=&subindex.front();
    // first shift
    unsigned pos=0; next_index(pos,it); wt += pos;
    *wt = extend(makepositive(jt->g,env)); ++jt;
    bool shortshifts=v64.size()<0xffff?true:checkshortshifts(subindex);
#ifdef GIAC_SHORTSHIFTTYPE
    if (shortshifts){
#if 1 && GIAC_SHORTSHIFTTYPE==16 && !defined BIGENDIAN
      if ( jt<jtend && ((ulonglong) it) & 0x2){ // align it to 32 bits
	wt += *it; ++it;
	*wt = extend(makepositive(jt->g,env));
        ++jt;
      }
      jtend-=2;
      for (;jt<=jtend;){
        unsigned * IT=(unsigned *) it;
	wt += ((*IT)&0xffff); 
	*wt = extend(makepositive(jt->g,env));
        ++jt;
	wt += ((*IT)>>16); 
	*wt = extend(makepositive(jt->g,env));
        ++jt;
        it+=2;
      }
      jtend+=2;
#endif
      for (;jt!=jtend;++jt){
	wt += *it; ++it;
	*wt = extend(makepositive(jt->g,env));
      }
    }
    else {
      for (;jt!=jtend;++jt){
	next_index(wt,it);
	*wt = extend(makepositive(jt->g,env));
      }
    }
#else // def GIAC_SHORTSHIFTTYPE
    for (;jt!=jtend;++jt){
      v64[*it] = extend(makepositive(jt->g,env));
      ++it; ++jt;
    }
#endif // def GIAC_SHORTSHIFTTYPE
  }    

  template<class tdeg_t,class modint_t>
  void zcollect_interreduce(const vectzpolymod<tdeg_t,modint_t> & res,const vector< unsigned > & G,vector<tdeg_t> & allf4buchberger,int start){
    vector< heap_tt<tdeg_t> > Ht;
    heap_tt<tdeg_t> heap_elem;
    vector< heap_tt_ptr<tdeg_t> > H; 
    Ht.reserve(2*G.size()+1);
    H.reserve(2*G.size());
    unsigned s=0;
    order_t keyorder={_REVLEX_ORDER,0};
    for (unsigned i=0;i<G.size();++i){
      const zpolymod<tdeg_t,modint_t> & p=res[G[i]];
      keyorder=p.order;
      if (int(p.coord.size())>start){
	s = giacmax(s, unsigned(p.coord.size()));
	Ht.push_back(heap_tt<tdeg_t>(true,i,start,(*p.expo)[p.coord[start].u]));
	H.push_back(heap_tt_ptr<tdeg_t>(&Ht.back()));
      }
    }
    allf4buchberger.reserve(s); // int(s*std::log(1+H.size())));
    compare_heap_tt_ptr<tdeg_t> key(keyorder);
    make_heap(H.begin(),H.end(),key);
    while (!H.empty()){
      // push root node of the heap in allf4buchberger
      heap_tt<tdeg_t> & current = *H.front().ptr;
      if (allf4buchberger.empty() || allf4buchberger.back()!=current.u)
	allf4buchberger.push_back(current.u);
      ++current.polymodpos;
      unsigned vpos;
      vpos=G[current.f4buchbergervpos];
      if (current.polymodpos>=res[vpos].coord.size()){
	std::pop_heap(H.begin(),H.end(),key);
	H.pop_back();
	continue;
      }
      const zpolymod<tdeg_t,modint_t> & resvpos=res[vpos];
      current.u=(*resvpos.expo)[resvpos.coord[current.polymodpos].u];
      // push_back &current into heap so that pop_heap will bubble out the
      // modified root node (initialization will exchange two identical pointers)
      H.push_back(heap_tt_ptr<tdeg_t>(&current));
      std::push_heap(H.begin(),H.end(),key); //?clang
      std::pop_heap(H.begin(),H.end(),key);
      H.pop_back();
    }
  }

  template<class tdeg_t,class modint_t,class modint_t2>
  int zf4mod(vectzpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,modint_t env,const vector< paire > & B,const vector<unsigned> * & permuBptr,vectzpolymod<tdeg_t,modint_t> & f4buchbergerv,bool learning,unsigned & learned_position,vector< paire > * pairs_reducing_to_zero,vector<zinfo_t<tdeg_t> > & f4buchberger_info,unsigned & f4buchberger_info_position,bool recomputeR,int age,bool multimodular,int parallel,int interreduce);

  template<class tdeg_t,class modint_t,class modint_t2>
  int zinterreduce_convert(vectzpolymod<tdeg_t,modint_t> & res,vector< unsigned > & G,modint_t env,bool learning,unsigned & learned_position,vector< paire > * pairs_reducing_to_zero,vector<zinfo_t<tdeg_t> > & f4buchberger_info,unsigned & f4buchberger_info_position,bool recomputeR,int age,bool multimodular,int parallel,vectpolymod<tdeg_t,modint_t> & resmod,bool interred){
    if (!interred)
      return 12345;
    if (res.empty()){ resmod.clear(); return 0; }
    order_t order=res.front().order;
    int dim=res.front().dim;
    unsigned Gs=G.size();
    // if (parallel<2 || Gs<200 || !threads_allowed ) return -1; // or fix in computeK1 non parallel case
    vector<paire> B; // not used
    const vector<unsigned> * permuBptr=0; // not used
    vectzpolymod<tdeg_t,modint_t> f4buchbergerv;
    int tmp=zf4mod<tdeg_t,modint_t,modint_t2>(res,G,env,B,permuBptr,f4buchbergerv,learning,learned_position,pairs_reducing_to_zero,f4buchberger_info,f4buchberger_info_position,recomputeR,age,multimodular,parallel,1);
    //CERR << "interreduce " << tmp << '\n';
    if (tmp<0 || tmp==12345) 
      return tmp;
    // build resmod from res leading monomial of res and f4buchbergerv
    ulonglong tot=0;
    for (unsigned i=0;i<Gs;++i)
      tot += f4buchbergerv[i].coord.size();
    // if (tot==0) return -1;
    for (unsigned i=0;i<Gs;++i){
      polymod<tdeg_t,modint_t> & q=resmod[G[i]];
      zpolymod<tdeg_t,modint_t> & p=f4buchbergerv[i]; 
      const vector<tdeg_t> & expo=*p.expo;
      q.dim=res[G[i]].dim;
      q.order=res[G[i]].order;
      q.fromleft=res[G[i]].fromleft;
      q.fromright=res[G[i]].fromright;
      q.age=res[G[i]].age;
      q.logz=res[G[i]].logz;
      q.coord.clear();
      q.coord.reserve(1+p.coord.size()); 
      if (res[G[i]].coord.empty()) 
	return -1;
      q.coord.push_back(T_unsigned<modint_t,tdeg_t>(res[G[i]].coord[0].g,(*res[G[i]].expo)[res[G[i]].coord[0].u]));
      for (unsigned j=0;j<p.coord.size();++j){
	modint_t g=p.coord[j].g;
	// g=(modint_t2(g)*coeff)%env;
	q.coord.push_back(T_unsigned<modint_t,tdeg_t>(g,expo[p.coord[j].u]));
      }
    }
    return 0;
  }

  template<class tdeg_t>
  void Rtorem(const vector<tdeg_t> & R,const vector<tdeg_t> & rem,vector<unsigned> & v){
    v.resize(R.size());
    typename vector<tdeg_t>::const_iterator it=R.begin(),itend=R.end(),jt=rem.begin(),jt0=jt,jtend=rem.end();
    vector<unsigned>::iterator vt=v.begin();
    for (;jt!=jtend;++jt){
      const tdeg_t & t=*jt;
      for (;it!=itend;++vt,++it){
	if (*it==t)
	  break;
      }
      *vt=hashgcd_U(jt-jt0);
    }
  }  

  template <class tdeg_t,class modint_t>
  struct thread_buchberger_t {
    const vectzpolymod<tdeg_t,modint_t> * resptr;
    vector< vector< modint_t> > * Kptr;
    const vector<unsigned> * G;
    const vector< paire > * Bptr;
    const vector<unsigned> * permuBptr;
    const vector<tdeg_t> *leftshiftptr,*rightshiftptr,*Rptr;
    void * Rhashptr;
    const vector<int> * Rdegposptr;
    modint_t env;
    int debut,fin,N,colonnes;
    const vector<unsigned> * firstposptr;
    const vector<vector<unsigned short> > * Mindexptr;
    const vector< vector<modint_t> > * Mcoeffptr;
    const vector<coeffindex_t> * coeffindexptr;
    vector< vector<shifttype> > * indexesptr;
    vector<used_t> * usedptr;
    unsigned * bitmap;
    bool displayinfo;
    bool learning;
    short int interreduce;
    const vector<paire> * pairs_reducing_to_zero; // read-only!
    int learned_position;
  };
  
  template <class tdeg_t,class modint_t,class modint_t2>
  void * thread_buchberger(void * ptr_){
    thread_buchberger_t<tdeg_t,modint_t> * ptr=(thread_buchberger_t<tdeg_t,modint_t> *) ptr_;
    const vectzpolymod<tdeg_t,modint_t> & res=*ptr->resptr;
    vector< vector<modint_t> > & K =*ptr->Kptr;
    const vector< paire > & B = *ptr->Bptr;
    const vector<unsigned> & G = *ptr->G;
    const vector<unsigned> & permuB = *ptr->permuBptr;
    const vector<tdeg_t> & leftshift=*ptr->leftshiftptr;
    const vector<tdeg_t> & rightshift=*ptr->rightshiftptr;
    const vector<tdeg_t> & R=*ptr->Rptr;
    void * Rhashptr=ptr->Rhashptr;
    const vector<int> & Rdegpos=*ptr->Rdegposptr;
    modint_t env=ptr->env;
    int debut=ptr->debut,fin=ptr->fin,N=ptr->N;
    const vector<unsigned> & firstpos=*ptr->firstposptr;
    int & colonnes=ptr->colonnes;
    const vector<vector<unsigned short> > &Mindex = *ptr->Mindexptr;
    const vector< vector<modint_t> > &Mcoeff = *ptr->Mcoeffptr;
    const vector<coeffindex_t> &coeffindex = *ptr->coeffindexptr;
    vector< vector<shifttype> > & indexes=*ptr->indexesptr;
    vector<used_t> & used = *ptr->usedptr;
    bool learning=ptr->learning;
    int interreduce=ptr->interreduce;
    int pos=ptr->learned_position;
    const vector<paire> * pairs_reducing_to_zero=ptr->pairs_reducing_to_zero;
    bool displayinfo=ptr->displayinfo;
    unsigned * bitmap=ptr->bitmap+debut*((N>>5)+1);
    vector<modint_t2> v64(N);
    unsigned bk_prev=-1;
    const tdeg_t * rightshift_prev =0;
    vector<modint_t> subcoeff2;
    int effi=-1,Bs=int(B.size());
    if (interreduce){
      // tdeg_t nullshift(res[G[0]].dim);
      for (int i=debut;i<fin;++i){
	if (interrupted || ctrl_c)
	  return 0;
	int index=interreduce==2?i+G.size():G[i],start=interreduce==2?0:1;
	zmakelinesplit(res[index],(const tdeg_t *) 0,R,Rhashptr,Rdegpos,indexes[i],0,start);
	zadd(v64,res[index],indexes[i],start,env);
	K[i].clear();
	int firstcol=indexes[i].empty()?0:indexes[i].front();
	colonnes=giacmin(colonnes,reducef4buchbergersplit(v64,Mindex,firstpos,firstcol,Mcoeff,coeffindex,K[i],bitmap,used,env));
	bitmap += (N>>5)+1;
      }
      return ptr_;
    }
    for (int i=debut;i<fin;++i){
      if (interrupted || ctrl_c)
	return 0;
      paire bk=B[permuB[i]];
      if (!learning && pairs_reducing_to_zero && pos<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[pos]){
	++pos;	
	continue;
      }
      // no learning with parallel
      zmakelinesplit(res[bk.first],&leftshift[permuB[i]],R,Rhashptr,Rdegpos,indexes[i],0,1);
      if (bk_prev!=bk.second || !rightshift_prev || *rightshift_prev!=rightshift[permuB[i]]){
	zmakelinesplit(res[bk.second],&rightshift[permuB[i]],R,Rhashptr,Rdegpos,indexes[Bs+i],0,1);
	bk_prev=bk.second;
	rightshift_prev=&rightshift[permuB[i]];
      }
    }
    bk_prev=-1; rightshift_prev=0;
    pos=ptr->learned_position;
    for (int i=debut;i<fin;++i){
      if (interrupted || ctrl_c)
	return 0;
      if (displayinfo){
	if (i%10==9) {COUT << "+"; COUT.flush(); }
	if (i%500==499) COUT << " " << CLOCK()*1e-6 << " remaining " << fin-i << '\n';
      }
      paire bk=B[permuB[i]];
      if (!learning && pairs_reducing_to_zero && pos<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[pos]){
	++pos;	
	unsigned tofill=(N>>5)+1;
	fill(bitmap,bitmap+tofill,0);
	bitmap += tofill;
	continue;
      }
      if (bk.second!=bk_prev || !rightshift_prev || *rightshift_prev!=rightshift[permuB[i]]){
	subcoeff2.clear();
	zcopycoeff(res[bk.second],subcoeff2,1);
	bk_prev=bk.second;
	rightshift_prev=&rightshift[permuB[i]];	  
      }
      // zcopycoeff(res[bk.first],subcoeff1,1);zadd(v64,subcoeff1,indexes[i]);
      zadd(v64,res[bk.first],indexes[i],1,env);
      effi=Bs+i;
      while (indexes[effi].empty() && effi)
	--effi;
      zsub(v64,subcoeff2,indexes[effi]);
      int firstcol=indexes[i].empty()?0:indexes[i].front();
      if (effi>=0 && !indexes[effi].empty())
	firstcol=giacmin(firstcol,indexes[effi].front());      
      K[i].clear();
      colonnes=giacmin(colonnes,reducef4buchbergersplit(v64,Mindex,firstpos,firstcol,Mcoeff,coeffindex,K[i],bitmap,used,env));
      bitmap += (N>>5)+1;
    }
    return ptr_;
  }

  template<class tdeg_t,class modint_t>
  struct pair_compare {
    const vector< paire > * Bptr;
    const vectzpolymod<tdeg_t,modint_t> * resptr ;
    const vector<tdeg_t> * leftshiftptr;
    const vector<tdeg_t> * rightshiftptr;
    order_t o;
    inline bool operator ()(unsigned a,unsigned b){
      unsigned Ba=(*Bptr)[a].second,Bb=(*Bptr)[b].second;
      const tdeg_t & adeg=(*resptr)[Ba].ldeg;
      const tdeg_t & bdeg=(*resptr)[Bb].ldeg;
      if (adeg!=bdeg)
	return tdeg_t_greater(bdeg,adeg,o)!=0; // return tdeg_t_greater(adeg,bdeg,o);
      const tdeg_t & aleft=(*rightshiftptr)[a];
      const tdeg_t & bleft=(*rightshiftptr)[b];
      return tdeg_t_strictly_greater(bleft,aleft,o);// return tdeg_t_strictly_greater(aleft,bleft,o);
    }
    pair_compare(const vector< paire > * Bptr_,
		 const vectzpolymod<tdeg_t,modint_t> * resptr_ ,
		 const vector<tdeg_t> * leftshiftptr_,
		 const vector<tdeg_t> * rightshiftptr_,
		 const order_t & o_):Bptr(Bptr_),resptr(resptr_),rightshiftptr(rightshiftptr_),leftshiftptr(leftshiftptr_),o(o_){}
  };

   // #define GIAC_CACHE2ND 1; // cache 2nd pair reduction, slower
  // Linear algebra is done in 2 steps: 1st step is reduce the s-pairs
  // wrt Mindex/Mcoeff in sparse linalg, without modification
  // then build a dense matrix and reduce it
  // It would be faster to do all at once, but that means modifying
  // Mindex/Mcoeff to add new reducers, would work well on 1 thread
  // but would probably not work well in parallel (memory locks)
  // and it would also be harder to trace reducers added.
  // This code is also optimized for memory. For this reason, coefficients
  // and indices storage are separated, because coefficients are the same
  // for each element of the gbasis multiplied by any element of the
  // corresponding quotient. Indices storage are also optimized in
  // memory using relative 2 bytes shifts instead of
  // 4 bytes absolute positions. The main loop
  // for sparse linear algebra (f4_innerloop_special_mod) must
  // maintain 2 iterators (instead of 1) and read in 3 areas
  // (instead of 2), this slows down the speed.
  // But the memory footprint is almost divided by a factor of 4 or 2:
  // instead of 1 modint=4 bytes + 1 absolute shift=4 bytes
  // or instead of 1 absolute shift=4 bytes
  // we have 1 relative shift=2 bytes
  template<class tdeg_t,class modint_t,class modint_t2>
  int zf4computeK1(const unsigned N,const unsigned nrows,const double mem,const unsigned Bs,vectzpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,modint_t env,const vector< paire > & B,const vector<unsigned> & permuB,bool learning,unsigned & learned_position,vector< paire > * pairs_reducing_to_zero,const vector<tdeg_t> & leftshift,const vector<tdeg_t> & rightshift, const vector<tdeg_t> & R ,void * Rhashptr,const vector<int> & Rdegpos,const vector<unsigned> &firstpos,vector<vector<unsigned short> > & Mindex, const vector<coeffindex_t> & coeffindex,vector< vector<modint_t> > & Mcoeff,zinfo_t<tdeg_t> * info_ptr,vector<used_t> &used,unsigned & usedcount,unsigned * bitmap,vector< vector<modint_t> > & K,int parallel,int interreduce){
    //parallel=1;
    bool freemem=mem>4e7; // should depend on real memory available
    bool large=N>8000;
    // CERR << "after sort " << Mindex << '\n';
    // step3 reduce
    unsigned colonnes=N;
    vector<modint_t> v(N);
    vector<modint_t2> v64(N);
    // vector<modint_t> v32(N);
    vector<double> v64d(N);
#if 0 // def x86_64
    vector<int128_t> v128;
    if (!large)
      v128.resize(N);
#endif
    unsigned Kcols=N-nrows;
    unsigned Ksizes=Kcols;
    if (info_ptr && !learning)
      Ksizes=giacmin(info_ptr->Ksizes+3,Kcols);
    bool Kdone=false;
#ifdef GIAC_CACHE2ND
    vector<modint_t2> subcoeff2;
#else
    vector<modint_t> subcoeff2;
#endif
    vector< vector<shifttype> > indexes(2*Bs);
#ifdef HAVE_LIBPTHREAD 
    if (Bs>=200 && threads_allowed && parallel>1 
	//&& (learning || !pairs_reducing_to_zero) 
	/*parallel*/){
      int th=giacmin(parallel,MAXNTHREADS)-1; // giacmin(threads,64)-1;
      vector<int> positions(1),learned_parallel(1);
      if (interreduce){
	for (unsigned i=0;i<Bs;++i){
	  indexes[i].reserve(res[(interreduce==2?i+G.size():G[i])].coord.size()+16);
	  K[i].reserve(Ksizes);
	}
	for (int i=1;i<parallel;++i){
	  positions.push_back((i*extend(Bs))/parallel);
	}
	positions.push_back(Bs);
	learned_parallel=positions; // not used
      }
      else { 
	// prepare memory and positions
	int pos=learned_position;
	for (unsigned i=0;i<Bs;++i){
	  if (pairs_reducing_to_zero && !learning && (*pairs_reducing_to_zero)[learned_position]==B[permuB[i]]){
	    ++learned_position;
	    continue;
	  }
	}
	// effective number of pairs to reduce is Bs-(learned_position-pos)
	int effBs=Bs-(learned_position-pos);
        parallel=giacmax(1,giacmin(effBs/32.*Ksizes/256.,parallel)); // effBs == real number of pairs to be reduced, IMPROVE parallel?
        th=giacmin(parallel,MAXNTHREADS)-1; // giacmin(threads,64)-1;
        int effstep=effBs/parallel+1,effi=0,effend=effstep;
	learned_parallel[0]=pos;
	// scan again pairs to set end positions and learned_position
	for (unsigned i=0;i<Bs;++i){
	  if (pairs_reducing_to_zero && !learning && (*pairs_reducing_to_zero)[pos]==B[permuB[i]]){
	    ++pos;
	    continue;
	  }
	  indexes[i].reserve(res[B[permuB[i]].first].coord.size()+16);
	  indexes[Bs+i].reserve(res[B[permuB[i]].second].coord.size()+16);
	  K[i].reserve(Ksizes);
	  ++effi;
	  if (effi>=effend){
	    positions.push_back(i); // end position for this thread
	    learned_parallel.push_back(pos); // learned position for next thread
	    effend += effstep;
	  }
	}
	// fix last pair number
	while (positions.size()<parallel)
	  positions.push_back(Bs); 
	while (learned_parallel.size()<parallel)
	  learned_parallel.push_back(pos); 
	if (positions.size()<parallel || learned_parallel.size()<parallel){
	  COUT << "BUG " << parallel << " " << positions.size() << " " << learned_parallel.size() << '\n' << positions << '\n' << learned_parallel << '\n';
	}
	if (positions.size()==parallel)
	  positions.push_back(Bs); 
	else
	  positions.back()=Bs;
      } // end else interreduce 
      pthread_t tab[MAXNTHREADS];
      thread_buchberger_t<tdeg_t,modint_t> buchberger_param[MAXNTHREADS];
      int colonnes=N;
      for (int j=0;j<=th;++j){
	thread_buchberger_t<tdeg_t,modint_t> tmp={&res,&K,&G,&B,&permuB,&leftshift,&rightshift,&R,Rhashptr,&Rdegpos,env,positions[j],positions[j+1],int(N),int(Kcols),&firstpos,&Mindex,&Mcoeff,&coeffindex,&indexes,&used,bitmap,j==th && debug_infolevel>1,learning,(short int)interreduce,pairs_reducing_to_zero,learned_parallel[j]};
	buchberger_param[j]=tmp;
	bool res=true;
	// CERR << "write " << j << " " << p << '\n';
	if (j<th)
	  res=pthread_create(&tab[j],(pthread_attr_t *) NULL,thread_buchberger<tdeg_t,modint_t,modint_t2>,(void *) &buchberger_param[j]);
	if (res)
	  thread_buchberger<tdeg_t,modint_t,modint_t2>((void *)&buchberger_param[j]);
      }
      Kdone=true;
      colonnes=buchberger_param[th].colonnes;
      for (unsigned j=0;j<th;++j){
	void * ptr_=(void *)&th; // non-zero initialisation
	pthread_join(tab[j],&ptr_);
	if (!ptr_)
	  Kdone=false;
	thread_buchberger_t<tdeg_t,modint_t> * ptr = (thread_buchberger_t<tdeg_t,modint_t> *) ptr_;
	colonnes=giacmin(colonnes,ptr->colonnes);
      }
    } // end parallelization
#endif
    if (!Kdone){
      if (interreduce){ 
	for (unsigned i=0;i<Bs;++i){
	  indexes[i].reserve(res[(interreduce==2?i+G.size():G[i])].coord.size()+16);
	  K[i].reserve(Ksizes);
	}
	thread_buchberger_t<tdeg_t,modint_t> tmp={&res,&K,&G,&B,&permuB,&leftshift,&rightshift,&R,Rhashptr,&Rdegpos,env,0,(int)Bs,int(N),int(Kcols),&firstpos,&Mindex,&Mcoeff,&coeffindex,&indexes,&used,bitmap,debug_infolevel>1,learning,(short int)interreduce,pairs_reducing_to_zero,0};
	thread_buchberger<tdeg_t,modint_t,modint_t2>((void *)&tmp);
	return 0;
      }
      unsigned bk_prev=-1;
      const tdeg_t * rightshift_prev=0;
      int pos=learned_position;
      for (unsigned i=0;i<Bs;i++){
	if (interrupted || ctrl_c)
	  return -1;
	paire bk=B[permuB[i]];
	if (!learning && pairs_reducing_to_zero && pos<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[pos]){
	  ++pos;	
	  continue;
	}
	bool done=false;
	const tdeg_t & curleft=leftshift[permuB[i]];
#ifdef GIAC_MAKELINECACHE // does not seem faster
	pair<int,int> ij=zmakelinecache[bk.first];
	if (ij.first!=-1){
	  // look in quo[ij.first] if leftshift[permuB[i]] is there, if true copy from Mindex
	  // except first index
	  typename std::vector<tdeg_t>::const_iterator cache_it=quo[ij.first].begin(),cache_end=quo[ij.first].end();
	  if (cache_it<cache_end && !dicho(cache_it,cache_end,curleft,order)){
	    if (cache_end-cache_it>5)
	      cache_it=cache_end;
	    else {
	      for (;cache_it<cache_end;++cache_it){
		if (*cache_it==curleft)
		  break;
	      }
	    }
	  }
	  if (cache_it<cache_end){
	    if (debug_infolevel>2)
	      CERR << "cached " << ij << '\n';
	    int pos=ij.second+cache_it-quo[ij.first].begin();
	    pos=permuM[pos];
	    vector<shifttype> & source=Mindex[pos];
	    vector<shifttype> & target=indexes[i];
	    target.reserve(source.size());
	    unsigned sourcepos=0,targetpos=0;
	    const shifttype * sourceptr=&source.front(),*sourceend=sourceptr+source.size();
	    next_index(sourcepos,sourceptr); // skip position 0
	    next_index(sourcepos,sourceptr);
	    pushsplit(target,targetpos,sourcepos);
	    for (;sourceptr<sourceend;++sourceptr){
	      target.push_back(*sourceptr);
	    }
	    //CERR << Mindex << '\n' << target << '\n';
	    done=true;
	  }
	}
#endif // GIAC_MAKELINECACHE
	if (!done){
	  indexes[i].reserve(res[bk.first].coord.size()+16);
	  zmakelinesplit(res[bk.first],&curleft,R,Rhashptr,Rdegpos,indexes[i],0,1);
	  //CERR << indexes[i] << '\n';
	}
	if (bk_prev!=bk.second || !rightshift_prev || *rightshift_prev!=rightshift[permuB[i]]){
	  indexes[Bs+i].reserve(res[bk.second].coord.size()+16);
	  zmakelinesplit(res[bk.second],&rightshift[permuB[i]],R,Rhashptr,Rdegpos,indexes[Bs+i],0,1);
	  bk_prev=bk.second;
	  rightshift_prev=&rightshift[permuB[i]];
	}
      } // end for (unsigned i=0;i<Bs;++i)
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " pairs indexes computed over " << R.size() << " monomials"<<'\n';
      bk_prev=-1; rightshift_prev=0;
      vector<modint_t> Ki; Ki.reserve(Ksizes);
      int effi=-1;
      for (unsigned i=0;i<Bs;++i){
	if (interrupted || ctrl_c)
	  return -1;
	if (debug_infolevel>1){
	  if (i%10==9) {COUT << "+"; COUT.flush(); }
	  if (i%500==499) COUT << " " << CLOCK()*1e-6 << " remaining " << Bs-i << '\n';
	}
	paire bk=B[permuB[i]];
	if (!learning && pairs_reducing_to_zero && learned_position<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[learned_position]){
	  if (debug_infolevel>2)
	    CERR << bk << " f4buchberger learned " << learned_position << '\n';
	  ++learned_position;
	  unsigned tofill=(N>>5)+1;
	  fill(bitmap,bitmap+tofill,0);
	  bitmap += tofill;
	  continue;
	}
	// zmakelinesub(res[bk.first],&leftshift[i],res[bk.second],&rightshift[i],R,v,1,env);
	// CERR << bk.first << " " << leftshift[i] << '\n';
	// v64.assign(N,0); // + reset v64 to 0, already done by zconvert_
	if (bk.second!=bk_prev || !rightshift_prev || *rightshift_prev!=rightshift[permuB[i]]){
	  subcoeff2.clear();
#ifdef GIAC_CACHE2ND
	  subcoeff2.resize(N);
	  zadd(subcoeff2,res[bk.second],indexes[i+Bs],1,env);
	  reducef4buchbergersplit(subcoeff2,Mindex,firstpos,0,Mcoeff,coeffindex,Ki,0 /* no bitmap, answer in subcoeff2 */,used,env);
#else
	  zcopycoeff(res[bk.second],subcoeff2,1);	  
#endif
	  bk_prev=bk.second;
	  rightshift_prev=&rightshift[permuB[i]];
	  if (effi>=0)
	    indexes[effi].clear();
	  effi=i+Bs;
	}
	int firstcol=indexes[i].empty()?0:indexes[i].front();
	if (effi>=0 && !indexes[effi].empty())
	  firstcol=giacmin(firstcol,indexes[effi].front());
	// zcopycoeff(res[bk.first],subcoeff1,1);zadd(v64,subcoeff1,indexes[i]);
	if (
#if defined(EMCC) || defined(EMCC2)
	    env>(1<<24) && env<=94906249
#else
	    0
#endif
	    ){
#ifndef GBASIS_4PRIMES
	  // using doubles instead of 64 bits integer not supported in JS
	  zadd(v64d,res[bk.first],indexes[i],1,env);
	  indexes[i].clear();
#ifdef GIAC_CACHE2ND
	  sub(v64d,subcoeff2);
#else
	  zsub(v64d,subcoeff2,indexes[effi]);
#endif
	  Ki.clear();
	  colonnes=giacmin(colonnes,reducef4buchbergersplitdouble(v64d,Mindex,firstpos,firstcol,Mcoeff,coeffindex,Ki,bitmap,used,env));
#endif
	}
	else {
#if 0 && defined PSEUDO_MOD && !defined BIGENDIAN && GIAC_SHORTSHIFTTYPE==16
          // this code is slower : the innerloop does more operations with pseudo-mod
	  zadd(v32,res[bk.first],indexes[i],1,env);
	  indexes[i].clear();
	  zsub(v32,subcoeff2,indexes[effi]);
	  Ki.clear();
	  colonnes=giacmin(colonnes,reducef4buchbergersplit32(v32,Mindex,firstpos,firstcol,Mcoeff,coeffindex,Ki,bitmap,used,env));
#else
	  zadd(v64,res[bk.first],indexes[i],1,env);
	  indexes[i].clear();
#ifdef GIAC_CACHE2ND
	  sub(v64,subcoeff2);
#else
	  zsub(v64,subcoeff2,indexes[effi]);
#endif
	  Ki.clear();
	  colonnes=giacmin(colonnes,reducef4buchbergersplit(v64,Mindex,firstpos,firstcol,Mcoeff,coeffindex,Ki,bitmap,used,env));
#endif // 32 bits intermediate vector
	}
	bitmap += (N>>5)+1;
	if (Ksizes<Kcols){
	  K[i].swap(Ki);
	  Ki.reserve(Ksizes);
	  continue;
	}
	size_t Kis=Ki.size();
	if (Kis>Ki.capacity()*.8){
	  K[i].swap(Ki);
	  Ki.reserve(giacmin(Kcols,int(Kis*1.1)));
	}
	else {
#if 0
	  vector<modint_t> & target=K[i];
	  target.reserve(giacmin(Kcols,int(Kis*1.1)));
	  vector<modint_t>::const_iterator kit=Ki.begin(),kitend=Ki.end();
	  for (;kit!=kitend;++kit)
	    target.push_back(*kit);
#else
	  K[i]=Ki;      
#endif
	}
	//CERR << v << '\n' << SK[i] << '\n';
      } // end for (i=0;i<B.size();++i)
    } // end if (!Kdone)
    // CERR << K << '\n';
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv split reduced " << Bs << " polynoms over " << N << " monomials, start at " << colonnes << '\n';
    return 0;
  }

  template<class tdeg_t,class modint_t>
  struct zbuildM_t {
    const vectzpolymod<tdeg_t,modint_t> * res;
    const vector<unsigned> * G;
    modint_t env;
    bool multimodular;
    const vector< vector<tdeg_t> > * quo;
    const vector<tdeg_t> * R;
    const vector<int> * Rdegpos;
    void * Rhashptr;
    vector<coeffindex_t> * coeffindex;
    unsigned N;
    vector<vector<unsigned short> > * Mindex;
    vector< vector<modint_t> > * Mcoeff;
    vector<sparse_element> * atrier;
    int i,iend,j;
  };

  template<class tdeg_t,class modint_t>
  void do_zbuildM(const vectzpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,modint_t env,bool multimodular,const vector< vector<tdeg_t> > & quo,const vector<tdeg_t> & R,const vector<int> & Rdegpos,void * Rhashptr,vector<coeffindex_t> & coeffindex,unsigned N,vector<vector<unsigned short> > & Mindex,vector< vector<modint_t> > & Mcoeff,vector<sparse_element> & atrier,int i,int iend,int j){
    for (;i<iend;++i){
      // copy coeffs of res[G[i]] in Mcoeff
      if (!quo[i].empty()) 
	zcopycoeff(res[G[i]],Mcoeff[i],0);
      // for each monomial of quo[i], find indexes and put in Mindex
      // reverse order traversing quo[i]
      // In zmakelinesplit locate res[G[i]].coord.u+*jt by dichotomoy 
      // between position just calculated before and 
      // and same position in previous Mindex
      typename std::vector< tdeg_t >::const_iterator jt=quo[i].end()-1;
      int quos=int(quo[i].size());
      int Gi=G[i];
      for (int k=quos-1;k>=0;--k,--jt){
	zmakelinesplit(res[Gi],&*jt,R,Rhashptr,Rdegpos,Mindex[j+k],k==quos-1?0:&Mindex[j+k+1],0);
      }
      for (int k=0;k<quos;++j,++k){
	coeffindex[j]=coeffindex_t(N<=0xffff,i);
	if (!coeffindex[j].b)
	  coeffindex[j].b=checkshortshifts(Mindex[j]);
	atrier[j]=sparse_element(first_index(Mindex[j]),j);
      }
    }
  }

  template<class tdeg_t,class modint_t>
  void * zbuildM_(void * ptr_){
    zbuildM_t<tdeg_t,modint_t> * ptr=(zbuildM_t<tdeg_t,modint_t> *) ptr_;
    do_zbuildM<tdeg_t,modint_t>(*ptr->res,*ptr->G,ptr->env,ptr->multimodular,*ptr->quo,*ptr->R,*ptr->Rdegpos,ptr->Rhashptr,*ptr->coeffindex,ptr->N,*ptr->Mindex,*ptr->Mcoeff,*ptr->atrier,ptr->i,ptr->iend,ptr->j);
    return ptr_;
  }

  template<class tdeg_t,class modint_t>
  void zbuildM(const vectzpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,modint_t env,bool multimodular,int parallel,const vector< vector<tdeg_t> > & quo,const vector<tdeg_t> & R,const vector<int> & Rdegpos,void * & Rhashptr,vector<coeffindex_t> & coeffindex,unsigned N,vector<vector<unsigned short> > & Mindex,vector< vector<modint_t> > & Mcoeff,vector<sparse_element> & atrier,int nrows){
#ifdef HAVE_LIBPTHREAD
#if 1 // IMPROVE parallel
    parallel=giacmax(1,giacmin(parallel,nrows/16));
#else
    if (nrows<16) parallel=1;
#endif
    pthread_t tab[parallel];
    zbuildM_t<tdeg_t,modint_t> zbuildM_param[parallel];
    int istart=0,iend=0,jstart=0,jend=0;
    for (int j=0;j<parallel;++j){
      if (j==parallel-1){
	iend=G.size();
      }
      else {
	for (iend=istart;iend<G.size();++iend){
	  jend += quo[iend].size();
	  if (jend>((j+1)*nrows/parallel)){
	    ++iend;
	    break;
	  }
	}
      }
      zbuildM_t<tdeg_t,modint_t> tmp={&res,&G,env,multimodular,&quo,&R,&Rdegpos,Rhashptr,&coeffindex,N,&Mindex,&Mcoeff,&atrier,istart,iend,jstart};
      zbuildM_param[j]=tmp;
      bool res=true;
      if (j<parallel-1)
	res=pthread_create(&tab[j],(pthread_attr_t *) NULL,zbuildM_<tdeg_t,modint_t>,(void *) &zbuildM_param[j]);
      if (res)
	zbuildM_<tdeg_t,modint_t>((void *)&zbuildM_param[j]);
      istart=iend;
      jstart=jend;
    }
    for (unsigned j=0;j<parallel-1;++j){
      void * ptr_=(void *)&parallel; // non-zero initialisation
      pthread_join(tab[j],&ptr_);
      if (!ptr_)
	CERR << "Error building M" << '\n';
    }
#else
    zbuildM_t<tdeg_t,modint_t> tmp={&res,&G,env,multimodular,&quo,&R,&Rdegpos,Rhashptr,&coeffindex,N,&Mindex,&Mcoeff,&atrier,0,int(G.size()),0};
    zbuildM_<tdeg_t,modint_t>((void *)&tmp);
#endif
  } // end parallelization

  template<class tdeg_t,class modint_t>
  int zf4denselinalg(vector<unsigned> & lebitmap,vector< vector<modint_t> > & K,modint_t env,vectzpolymod<tdeg_t,modint_t> & f4buchbergerv,zinfo_t<tdeg_t> * info_ptr,vector<unsigned> & Rtoremv,unsigned N,unsigned Bs,unsigned nrows,vector<used_t> &used,unsigned usedcount,double mem,const order_t &order,int dim,int age,bool learning,bool multimodular,int parallel,int interreduce){
    //parallel=1;
    // create dense matrix K 
    unsigned * bitmap=&lebitmap.front();
    unsigned zeros=create_matrix(bitmap,(N>>5)+1,used,K);
    // clear memory required for lescoeffs
    { vector<unsigned> tmp1; lebitmap.swap(tmp1); }
    if (debug_infolevel>1){
      CERR << CLOCK()*1e-6 << " nthreads=" << parallel << " dense_rref " << K.size()-zeros << "(" << K.size() << ")" << "x" << usedcount << " ncoeffs=" << double(K.size()-zeros)*usedcount*1e-6 << "*1e6\n";
      double nz=0,nzrow=0;
      for (unsigned i=0;i<K.size();++i){
	vector<modint_t> & Ki=K[i];
	if (!Ki.size())
	  continue;
	nzrow+=usedcount;
	for (unsigned j=0;j<Ki.size();++j){
	  if (Ki[j]!=0)
	    ++nz;
	}
      }
      CERR << "non-0 percentage " << nz/nzrow << '\n';
    }
    if (0 && !learning && info_ptr->permu.size()==Bs){
      vector<int> permutation=info_ptr->permu;
      vector< vector<modint_t> > K1(Bs);
      for (unsigned i=0;i<Bs;++i){
	swap(K1[i],K[permutation[i]]);
      }
      swap(K1,K);
    }
    vecteur pivots; vector<int> permutation,maxrankcols; longlong idet;
    int th=giacmin(parallel,MAXNTHREADS)-1; // giacmin(threads,64)-1;
    if (interreduce){ // interreduce==true means final interreduction
      ;
    }
    else {
      smallmodrref(parallel,K,pivots,permutation,maxrankcols,idet,0,int(K.size()),0,usedcount,0/* lower reduction*/,0/*dontswapbelow*/,env,0/* rrefordetorlu*/,permutation.empty()/* reset */,0,!multimodular/* allow_block*/,-1);
      // FIXME allow_block fails with parallel>1
      //smallmodrref(parallel,K,pivots,permutation,maxrankcols,idet,0,int(K.size()),0,usedcount,0/* lower reduction*/,0/*dontswapbelow*/,env,0/* rrefordetorlu*/,permutation.empty()/* reset */,0,true,-1); 
      if (1){
	if (debug_infolevel>1)
	  CERR << CLOCK()*1e-6 << " rref_upper " << '\n';
	int Ksize=int(K.size());
	if (//1
	    usedcount<=2*Ksize 
	    || parallel==1 || Ksize<50
	    )
	  smallmodrref_upper(K,0,Ksize,0,usedcount,env);
	else { 
	  thread_smallmodrref_upper(K,0,Ksize,0,usedcount,env,parallel);
	}
      }
    } // end if !interreduce
    unsigned Kcols=N-nrows;
    free_null_lines(K,0,Bs,0,Kcols);
    unsigned first0 = unsigned(pivots.size());
    int i;
    if (!interreduce && first0<K.size() && learning){
      vector<modint_t> & tmpv=K[first0];
      for (i=0;i<tmpv.size();++i){
	if (tmpv[i]!=0)
	  break;
      }
      if (i==tmpv.size()){
	unsigned Ksize = unsigned(K.size());
	K.resize(first0);
	K.resize(Ksize);
      }
    }
    //CERR << permutation << K << '\n';
    if (!learning){
      // ??? check that permutation is the same as learned permutation
      vector<int> permutation;
      bool copy=false;
      for (unsigned j=0;j<permutation.size();++j){
	if (permutation[j]!=info_ptr->permu[j]){
	  copy=true;
	  if (K[permutation[j]].empty() && K[info_ptr->permu[j]].empty())
	    continue;
	  CERR << "learning failed"<<'\n';
	  return -1;
	}
      }
      if (copy) 
	permutation=info_ptr->permu;
    }
    if (learning)
      info_ptr->permu=permutation;
    // CERR << K << "," << permutation << '\n';
    // vector<int> permu=perminv(permutation);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv interreduced" << '\n';
    for (i=0;i<f4buchbergerv.size();++i){
      // CERR << v << '\n';
      int pi=interreduce?i:permutation[i];
      f4buchbergerv[pi].expo=&info_ptr->rem;
      f4buchbergerv[pi].order=order;
      f4buchbergerv[pi].dim=dim;
      f4buchbergerv[pi].age=age;
      vector<  T_unsigned<modint_t,unsigned>  > & Pcoord=f4buchbergerv[pi].coord;
      Pcoord.clear();
      vector<modint_t> & v =K[i];
      if (v.empty()){
	continue;
      }
      unsigned vcount=0;
      typename vector<modint_t>::const_iterator vt=v.begin(),vtend=v.end();
      for (;vt!=vtend;++vt){
	if (*vt!=0)
	  ++vcount;
      }
      Pcoord.reserve(vcount);
      vector<used_t>::const_iterator ut=used.begin();
      unsigned pos=0;
      for (vt=v.begin();pos<N;++ut,++pos){
	if (!*ut)
	  continue;
	modint_t coeff=*vt;
	++vt;
	if (coeff!=0)
	  Pcoord.push_back( T_unsigned<modint_t,unsigned> (coeff,Rtoremv[pos]));
      }
      if (!Pcoord.empty())
	f4buchbergerv[pi].ldeg=(*f4buchbergerv[pi].expo)[Pcoord.front().u];
      if (!interreduce && !Pcoord.empty() && ( (env > (1<< 24)) || Pcoord.front().g!=1) ){
	zsmallmultmod(invmod(Pcoord.front().g,env),f4buchbergerv[pi],env);	
	Pcoord.front().g=1;
      }
      bool freemem=mem>4e7; // should depend on real memory available
      if (freemem){
	vector<modint_t> tmp; tmp.swap(v);
      }
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv stored" << '\n';
    return 1;
  }

  void extract(const vector< vector<mod4int> > & K,vector< vector<modint> > & K0,int pos){
    K0.resize(K.size());
    for (int i=0;i<K.size();++i){
      const vector<mod4int> & Ki=K[i];
      vector<modint> & K0i=K0[i];
      int s=Ki.size();
      K0i.reserve(s); K0i.clear();
      for (int j=0;j<s;++j)
       K0i.push_back(Ki[j].tab[pos]);
    }
  }

#if 1
  template<class tdeg_t>
  int zf4denselinalg(vector<unsigned> & lebitmap,vector< vector<mod4int> > & K4,mod4int env,vectzpolymod<tdeg_t,mod4int> & f4buchbergerv,zinfo_t<tdeg_t> * info_ptr,vector<unsigned> & Rtoremv,unsigned N,unsigned Bs,unsigned nrows,vector<used_t> &used,unsigned usedcount,double mem,const order_t &order,int dim,int age,bool learning,bool multimodular,int parallel,int interreduce){
    unsigned * bitmap=&lebitmap.front();
    vector< vector<modint> > K;
    int last_line=-1; vector<int> permutation0;
    if (!learning && info_ptr)
      permutation0=info_ptr->permu;
    for (int pos=0;pos<sizeof(mod4int)/sizeof(modint);++pos){
      // create dense matrix K 
      extract(K4,K,pos);
      unsigned zeros=create_matrix(bitmap,(N>>5)+1,used,K);
      if (!permutation0.empty()){
        // apply permutation0 and clear lines that reduced to 0 for prime at pos=0
        apply_permutation(K,permutation0);
        if (pos>0){
          for (int l=last_line+1;l<permutation0.size();++l){
            K[l].clear();
          }
        }
      }
      if (debug_infolevel>1){
        CERR << CLOCK()*1e-6 << " dense_rref[0] " << K.size()-zeros << "(" << K.size() << ")" << "x" << usedcount << " ncoeffs=" << double(K.size()-zeros)*usedcount*1e-6 << "*1e6\n";
        double nz=0,nzrow=0;
        for (unsigned i=0;i<K.size();++i){
          vector<int> & Ki=K[i];
          if (!Ki.size())
            continue;
          nzrow+=usedcount;
          for (unsigned j=0;j<Ki.size();++j){
            if (Ki[j]) ++nz;
          }
        }
        CERR << "non-0 percentage " << nz/nzrow << '\n';
      }
      vecteur pivots; vector<int> permutation,maxrankcols; longlong idet;
      int th=giacmin(parallel,MAXNTHREADS)-1; // giacmin(threads,64)-1;
      if (!interreduce){
        smallmodrref(parallel,K,pivots,permutation,maxrankcols,idet,0,int(K.size()),0,usedcount,0/* lower reduction*/,0/*dontswapbelow*/,env.tab[pos],0/* rrefordetorlu*/,permutation.empty()/* reset */,0,!multimodular,-1); 
        //smallmodrref(parallel,K,pivots,permutation,maxrankcols,idet,0,int(K.size()),0,usedcount,0/* lower reduction*/,0/*dontswapbelow*/,env.tab[pos],0/* rrefordetorlu*/,permutation.empty()/* reset */,0,true,-1);
        if (permutation0.empty())
          permutation0=permutation;
        else { // check for identity permutation
          for (int j=0;j<permutation.size();++j){
            if (permutation[j]!=j){
	      int k;
	      vector<int> Kchk=K[permutation[j]];
	      for (k=0;k<Kchk.size();++k){
		if (Kchk[k])
		  break;
	      }
	      if (k<Kchk.size()){
		CERR << "denselinalg mod4int learning failed"<<'\n';
		return 0;
	      }
            }
          }
          permutation=permutation0;
        }
        if (1){
          if (debug_infolevel>1)
            CERR << CLOCK()*1e-6 << " rref_upper " << '\n';
          int Ksize=int(K.size());
          if (//1
              usedcount<=2*Ksize 
              || parallel==1 || Ksize<50
              )
            smallmodrref_upper(K,0,Ksize,0,usedcount,env.tab[pos]);
          else { 
            thread_smallmodrref_upper(K,0,Ksize,0,usedcount,env.tab[pos],parallel);
          }
        }
      } // end if !interreduce
      unsigned Kcols=N-nrows;
      if (pos==0){ // set last non 0 line of K for next primes
        permutation0=permutation; // save for pos 1 to 3
        for (last_line=K.size()-1;last_line>=0;--last_line){
          int C;
          vector<int> & KL=K[last_line];
          for (C=KL.size()-1;C>=0;--C){
            if (KL[C]) break;
          }
          if (C>=0)
            break;
        }
      }
      unsigned first0 = unsigned(pivots.size());
      int i;
      if (!interreduce && first0<K.size() && learning){
        vector<modint> & tmpv=K[first0];
        for (i=0;i<tmpv.size();++i){
          if (tmpv[i])
            break;
        }
        if (i==tmpv.size()){
          unsigned Ksize = unsigned(K.size());
          K.resize(first0);
          K.resize(Ksize);
        }
      }
      //CERR << permutation << K << '\n';
      if (learning)
        info_ptr->permu=permutation;
      // CERR << K << "," << permutation << '\n';
      // vector<int> permu=perminv(permutation);
      if (debug_infolevel>1)
        CERR << CLOCK()*1e-6 << " f4buchbergerv interreduced" << '\n';
      for (i=0;i<f4buchbergerv.size();++i){
        // CERR << v << '\n';
        int pi=interreduce?i:permutation[i];
        if (pos==0){
          f4buchbergerv[pi].expo=&info_ptr->rem;
          f4buchbergerv[pi].order=order;
          f4buchbergerv[pi].dim=dim;
          f4buchbergerv[pi].age=age;
        }
        vector< T_unsigned<mod4int,unsigned>  > & Pcoord=f4buchbergerv[pi].coord;
        if (pos==0)
          Pcoord.clear();
        vector<modint> & v =K[i];
        if (v.empty())
          continue;
        if (pos==0){
          Pcoord.reserve(v.size());
          for (int i=0;i<v.size();++i)
            Pcoord.push_back(T_unsigned<mod4int,unsigned>(create<mod4int>(0),0));
        }
        vector<used_t>::const_iterator ut=used.begin();
        typename vector<modint>::const_iterator vt=v.begin(),vtend=v.end();
        unsigned pcoordpos=0;
        for (vt=v.begin();vt!=vtend;++ut){
          if (!*ut)
            continue;
          modint coeff=*vt; ++vt;
          Pcoord[pcoordpos].g.tab[pos]=coeff;
          if (pos==0)
            Pcoord[pcoordpos].u=Rtoremv[ut-used.begin()];
          ++pcoordpos;
        }
        if (pos==sizeof(mod4int)/sizeof(modint)-1){
          vector< T_unsigned<mod4int,unsigned>  > trimPcoord;
          unsigned vcount=0;
          vector< T_unsigned<mod4int,unsigned>  > ::const_iterator Pit=Pcoord.begin(),Pitend=Pcoord.end();
          for (;Pit!=Pitend;++Pit)
            if (!is_zero(Pit->g))
              ++vcount;
          trimPcoord.reserve(vcount);
          for (Pit=Pcoord.begin();Pit!=Pitend;++Pit)
            if (!is_zero(Pit->g))
              trimPcoord.push_back(*Pit);
          trimPcoord.swap(Pcoord);
          if (!Pcoord.empty())
            f4buchbergerv[pi].ldeg=(*f4buchbergerv[pi].expo)[Pcoord.front().u];
          if (!interreduce && !Pcoord.empty() && ( (env > (1<< 24)) || Pcoord.front().g!=1) ){
            zsmallmultmod(invmod(Pcoord.front().g,env),f4buchbergerv[pi],env);	
            Pcoord.front().g=create<mod4int>(1);
          }
        }
      }
    } // end for loop on pos
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " f4buchbergerv stored" << '\n';
    return 1;
  }
#endif

  // interreduce==0 normal F4 algo reduction, ==1 final gb auto-interreduction
  // to be done ==2 reduction of res[G.size()...] by gbasis in res, 
  // G should be identity, res[0] to res[G.size()-1] the gbasis
  template<class tdeg_t,class modint_t,class modint_t2>
  int zf4mod(vectzpolymod<tdeg_t,modint_t> & res,const vector<unsigned> & G,modint_t env,const vector< paire > & B,const vector<unsigned> * & permuBptr,vectzpolymod<tdeg_t,modint_t> & f4buchbergerv,bool learning,unsigned & learned_position,vector< paire > * pairs_reducing_to_zero,vector<zinfo_t<tdeg_t> > & f4buchberger_info,unsigned & f4buchberger_info_position,bool recomputeR,int age,bool multimodular,int parallel,int interreduce){
    unsigned Bs=unsigned(interreduce?(interreduce==2?res.size()-G.size():G.size()):B.size());
    if (!Bs)
      return 0;
    vector<unsigned> G2; 
    if (interreduce==2){
      for (unsigned i=G.size();i<res.size();++i)
	G2.push_back(i);
    }
    int dim=res.front().dim;
    order_t order=res.front().order;
    vector<tdeg_t> leftshift(Bs);
    vector<tdeg_t> rightshift(Bs);
    if (!interreduce) 
      zleftright(res,B,leftshift,rightshift);
    // IMPROVEMENT: sort pairs in B according to right term of the pair
    // If several pairs share the same right term, 
    // reduce the right term without leading monomial once
    // reduce corresponding left terms without leading monomial 
    // subtract
    f4buchbergerv.resize(Bs);
    zinfo_t<tdeg_t> info_tmp;
    unsigned nonzero = unsigned(Bs);
    zinfo_t<tdeg_t> * info_ptr=0;
    if (!learning && f4buchberger_info_position<f4buchberger_info.size()){
      info_ptr=&f4buchberger_info[f4buchberger_info_position];
      ++f4buchberger_info_position;
      nonzero=info_ptr->nonzero;
      if (nonzero==0 && !interreduce){
	for (int i=0;i<f4buchbergerv.size();++i){
	  // CERR << v << '\n';
	  f4buchbergerv[i].expo=&info_ptr->rem;
	  f4buchbergerv[i].order=order;
	  f4buchbergerv[i].dim=dim;
	  vector<  T_unsigned<modint_t,unsigned>  > & Pcoord=f4buchbergerv[i].coord;
	  Pcoord.clear();
	}
	return 1;
      }
    }
    else {
      vector<tdeg_t> all;
      vector<unsigned> permuB(Bs);
      for (unsigned i=0;i<Bs;++i) 
	permuB[i]=i;
#if 1 // not required for one modular gbasis, but kept for multi-modular
      if (!interreduce){
	pair_compare<tdeg_t,modint_t> trieur(&B,&res,&leftshift,&rightshift,order);
	sort(permuB.begin(),permuB.end(),trieur);
	if (debug_infolevel>2){
	  unsigned egales=0;
	  for (unsigned i=1;i<Bs;++i){
	    if (B[permuB[i-1]].second!=B[permuB[i]].second)
	      continue;
	    if (rightshift[permuB[i-1]]==rightshift[permuB[i]]){
	      egales++;
	      CERR << B[permuB[i-1]] << "=" << B[permuB[i]] << '\n';
	    }
	  }
	  CERR << egales << " right pair elements are the same" << '\n';
	  CERR << CLOCK()*1e-6 << " zf4buchberger begin collect monomials on #polys " << f4buchbergerv.size() << '\n';
	}
      }
#endif
      if (interreduce){
	if (interreduce==2){
	  zcollect_interreduce(res,G2,all,0); // all monomials 
	}
	else
	  zcollect_interreduce(res,G,all,1); // all monomials after leading one
      }
      else {
	if (!zcollect(res,B,permuB,all,leftshift,rightshift))
	  return -1;
      }
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " zf4buchberger symbolic preprocess" << '\n';
      zsymbolic_preprocess(all,res,G,-1,info_tmp.quo,info_tmp.rem,info_tmp.R);
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " zend symbolic preprocess" << '\n';
#if 0
      f4buchberger_info->push_back(*info_ptr);
#else
      zinfo_t<tdeg_t> tmp; tmp.nonzero=0; tmp.Ksizes=0;
      f4buchberger_info.push_back(tmp);
      zinfo_t<tdeg_t> & i=f4buchberger_info.back();
      swap(i.quo,info_tmp.quo);
      swap(i.R,info_tmp.R);
      swap(i.rem,info_tmp.rem);
      swap(i.permuB,permuB);
      info_ptr=&f4buchberger_info.back();
#endif
    }
    const vector<unsigned> & permuB =info_ptr->permuB ;
    permuBptr=&permuB;
    const vector<tdeg_t> & R = info_ptr->R;
    vector<unsigned> Rtoremv;
    Rtorem(R,info_ptr->rem,Rtoremv); // positions of R degrees in rem
    const vector< vector<tdeg_t> > & quo = info_ptr->quo;
    //CERR << quo << '\n';
    unsigned N = unsigned(R.size()), i, j = 0;
    if (N==0) return 1;
    void * Rhashptr=0;
#ifdef GIAC_RHASH // default disabled
    tdeg_t64_hash_t Rhash; 
    if (R.front().vars64()){
      Rhashptr=&Rhash;
      //Rhash.reserve(N);
      for (unsigned i=0;i<N;++i){
	R[i].add_to_hash(Rhashptr,i);
      }
    }
#endif
    int Rcurdeg=R.front().tdeg;
    vector<int> Rdegpos(Rcurdeg+2);
#ifdef GIAC_RDEG // default enabled
    for (unsigned i=0;i<N;++i){
      int tmp=R[i].tdeg;
      if (tmp==Rcurdeg)
	continue;
      for (;Rcurdeg>tmp;--Rcurdeg){
	Rdegpos[Rcurdeg]=i;
      }
    }
    for (;Rcurdeg>=0;--Rcurdeg){
      Rdegpos[Rcurdeg]=N;
    }
#endif
    unsigned nrows=0;
    for (i=0;i<G.size();++i){
      nrows += unsigned(quo[i].size());
    }
    if (nrows==0 && interreduce==1){
      // allready interreduced, nothing to do...
      info_ptr->nonzero=G.size();
      return 12345; // special code, already interreduced
    }
    double sknon0=0;
    unsigned usedcount=0,zerolines=0;
    vector< vector<modint_t> > K(Bs);
    vector<vector<unsigned short> > Mindex;
    vector< vector<modint_t> > Mcoeff(G.size());
    vector<coeffindex_t> coeffindex(nrows);
    Mindex.reserve(nrows);
    vector<sparse_element> atrier(nrows);
    // atrier.reserve(nrows);
    for (i=0;i<G.size();++i){
      typename std::vector<tdeg_t>::const_iterator jt=quo[i].begin(),jtend=quo[i].end();
      if (jt!=jtend)
	Mcoeff[i].reserve(res[G[i]].coord.size());
      for (;jt!=jtend;++j,++jt){
	Mindex.push_back(vector<unsigned short>(0));
	Mindex[j].reserve(int(1.1*res[G[i]].coord.size()));
      }
    }
#ifndef GIAC_MAKELINECACHE
    zbuildM(res,G,env,multimodular,parallel,quo,R,Rdegpos,Rhashptr,coeffindex,N,Mindex,Mcoeff,atrier,nrows);
#else // ZBUILDM
#ifdef GIAC_MAKELINECACHE
    vector< pair<int,int> > zmakelinecache(res.size(),pair<int,int>(-1,-1)); // -1 if res[k] is not in G, (i,j) if k==G[i] where j is the first index in Mindex of the part corresponding to res[G[i]]
#endif
    for (i=0,j=0;i<G.size();++i){
      // copy coeffs of res[G[i]] in Mcoeff
      if (1 || env<(1<<24)){
	if (!quo[i].empty()) 
	  zcopycoeff(res[G[i]],Mcoeff[i],0);
      }
      else
	zcopycoeff(res[G[i]],Mcoeff[i],env,0);
      // if (!Mcoeff[i].empty()) Mcoeff[i].front()=invmod(Mcoeff[i].front(),env);
      // for each monomial of quo[i], find indexes and put in Mindex
      // Improvement idea: reverse order traversing quo[i]
      // In zmakelinesplit locate res[G[i]].coord.u+*jt by dichotomoy 
      // between position just calculated before and 
      // and same position in previous Mindex
#if 1
      typename std::vector< tdeg_t >::const_iterator jt=quo[i].end()-1;
      int quos=int(quo[i].size());
      int Gi=G[i];
#ifdef GIAC_MAKELINECACHE
      zmakelinecache[Gi]=pair<int,int>(i,j);
#endif
      for (int k=quos-1;k>=0;--k,--jt){
	zmakelinesplit(res[Gi],&*jt,R,Rhashptr,Rdegpos,Mindex[j+k],k==quos-1?0:&Mindex[j+k+1],0);
      }
      for (int k=0;k<quos;++j,++k){
	coeffindex[j]=coeffindex_t(N<=0xffff,i);
	if (!coeffindex[j].b)
	  coeffindex[j].b=checkshortshifts(Mindex[j]);
	// atrier.push_back(sparse_element(first_index(Mindex[j]),j));
	atrier[j]=sparse_element(first_index(Mindex[j]),j);
      }
#else
      typename std::vector< tdeg_t >::const_iterator jt=quo[i].begin(),jtend=quo[i].end();
      for (;jt!=jtend;++j,++jt){
	coeffindex[j]=coeffindex_t(N<=0xffff,i);
	zmakelinesplit(res[G[i]],&*jt,R,Rhashptr,Rdegpos,Mindex[j],0,0);
	if (!coeffindex[j].b)
	  coeffindex[j].b=checkshortshifts(Mindex[j]);
	// atrier.push_back(sparse_element(first_index(Mindex[j]),j));
	atrier[j]=sparse_element(first_index(Mindex[j]),j);
      }
#endif
    }
#endif // ZBUILM
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " end build Mindex/Mcoeff zf4mod" << '\n';
    // should not sort but compare res[G[i]]*quo[i] monomials to build M already sorted
    // CERR << "before sort " << Mindex << '\n';
    sort_vector_sparse_element(atrier.begin(),atrier.end()); // sort(atrier.begin(),atrier.end(),tri1); 
    vector<coeffindex_t> coeffindex1(atrier.size());
    double mem=0; // mem*4=number of bytes allocated for M1
    vector< vector<unsigned short> > Mindex1(atrier.size());
#ifdef GIAC_MAKELINECACHE
    vector<int> permuM(atrier.size());
#endif
    for (i=0;i<atrier.size();++i){
      swap(Mindex1[i],Mindex[atrier[i].pos]);
      mem += Mindex1[i].size();
      swap(coeffindex1[i],coeffindex[atrier[i].pos]);
#ifdef GIAC_MAKELINECACHE
      permuM[atrier[i].pos]=i;
#endif
    }
    swap(Mindex,Mindex1);
    nrows = unsigned(Mindex.size());
    swap(coeffindex,coeffindex1);
    vector<unsigned> firstpos(atrier.size());
    for (i=0;i < atrier.size();++i){
      firstpos[i]=atrier[i].val;
    }
    double ratio=(mem/nrows)/N;
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " Mindex sorted, rows " << nrows << " columns " << N << " terms " << mem << " ratio " << ratio <<'\n';
    if (N<nrows){
      CERR << "Error " << N << "," << nrows << '\n';
      return -1;
    }
    // ((N>>5)+1)*Bs should not exceed 2e9 otherwise this will segfault
    if (double(Bs)*(N>>5)>2e9){
      CERR << "Error, problem too large. Try again after running gbasis_max_pairs(n) with n<" << 2e9/(N>>5) << '\n';
      return -1;
    }
    vector<used_t> used(N,0);
    vector<unsigned> lebitmap(((N>>5)+1)*Bs);
    unsigned * bitmap=&lebitmap.front();
    int zres=zf4computeK1<tdeg_t,modint_t,modint_t2>(N,nrows,mem,Bs,res,G,env, B,permuB,learning,learned_position,pairs_reducing_to_zero,leftshift,rightshift,  R ,Rhashptr,Rdegpos,firstpos,Mindex, coeffindex,Mcoeff,info_ptr,used,usedcount,bitmap,K,parallel,interreduce);
    if (zres!=0)
      return zres;
    if (debug_infolevel>1){
      CERR << '\n' << CLOCK()*1e-6 << " Memory usage: " << memory_usage()*1e-6 << "M" << '\n';
    }
    size_t Mindexsize=Mindex.size();
    Mindex.clear();
    Mcoeff.clear();
    {
      vector<vector<unsigned short> > Mindexclear;
      vector< vector<modint_t> > Mcoeffclear;
      Mindex.swap(Mindexclear);
      Mcoeff.swap(Mcoeffclear);
    }
    if (!pairs_reducing_to_zero){
      vector<tdeg_t> clearer;
      info_ptr->R.swap(clearer);
    }
    for (unsigned i=0;i<N;++i)
      usedcount += (used[i]>0);
    if (learning && info_ptr)
      info_ptr->Ksizes=usedcount;
    if (debug_infolevel>1){
      CERR << CLOCK()*1e-6 << " number of non-zero columns " << usedcount << " over " << N-Mindexsize << " (N " << N  << ", Mindex size " << Mindexsize << ")" << '\n'; // usedcount should be approx N-M.size()=number of cols of M-number of rows
      if (debug_infolevel>3)
	CERR << " column split used " << used << '\n';
    }
    //vector<modint_t> tmp; lescoeffs.swap(tmp); 
    return zf4denselinalg(lebitmap,K,env,f4buchbergerv,info_ptr,Rtoremv,N,Bs,nrows,used,usedcount,mem,order,dim,age,learning,multimodular,parallel,interreduce);
  }

  template<class tdeg_t,class modint_t,class modint_t2>
  int zsimult_reduce(vector< polymod<tdeg_t,modint_t> >  & v,const vector< polymod<tdeg_t,modint_t> > & gbmod,int env,bool multimodular,int parallel){
    if (v.empty()){ return 0; }
    vectpolymod<tdeg_t,modint_t> all; all.reserve(gbmod.size()+v.size()); polymod<tdeg_t,modint_t> TMP1; 
    for (int i=0;i<gbmod.size();++i)
      all.push_back(gbmod[i]);
    for (int i=0;i<v.size();++i)
      all.push_back(v[i]);
    collect(all,TMP1);
    // R0 stores monomials 
    vector<tdeg_t> R0(TMP1.coord.size());
    for (unsigned l=0;l<TMP1.coord.size();++l)
      R0[l]=TMP1.coord[l].u;
    vectzpolymod<tdeg_t,modint_t> zall; zall.resize(all.size());
    for (unsigned l=0;l<all.size();++l){
      convert(all[l],zall[l],R0);
      zsmallmultmod(1,zall[l],env);
    }
    vector< unsigned > G;
    for (int i=0;i<gbmod.size();++i){
      G.push_back(i);
    }
    order_t order=zall.front().order;
    int dim=zall.front().dim;
    unsigned Gs=G.size();
    // if (parallel<2 || Gs<200 || !threads_allowed ) return -1; // or fix in computeK1 non parallel case
    vector<paire> B; // not used
    const vector<unsigned> * permuBptr=0; // not used
    vector<zinfo_t<tdeg_t> > f4buchberger_info;unsigned f4buchberger_info_position=0;
    vectzpolymod<tdeg_t,modint_t> f4buchbergerv;
    bool learning=false;unsigned learned_position=0;
    vector< paire > * pairs_reducing_to_zero=0;
    bool recomputeR=false; int age=0;
    int tmp=zf4mod<tdeg_t,modint_t,modint_t2>(
		   zall,G,env,B,permuBptr,f4buchbergerv,learning,learned_position,pairs_reducing_to_zero,f4buchberger_info,f4buchberger_info_position,recomputeR,age,multimodular,parallel,2);
    //CERR << "interreduce " << tmp << '\n';
    if (tmp<0 || tmp==12345) 
      return tmp;
    for (unsigned i=0;i<v.size();++i){
      convert(f4buchbergerv[i],v[i]); // ,f4buchberger_info.back().R);
    }	  
    return 0;
  }

  /*
Let {f1, ..., fr} be a set of polynomials. The Gebauer-Moller Criteria are as follows:
1. Criterion M holds for a pair (fi, fk) if ∃ j < k, such that
   LCM{LM(fj),LM(fk)} properly divides LCM{LM(fi),LM(fk)}
2. Criterion F holds for a pair (fi, fk) if ∃ j < i, such that
   LCM{LM(fj),LM(fk)} = LCM{LM(fi),LM(fk)}.
3. Criterion Bk holds for a pair (fi, fj) if ∃ j < k and
   LM(fk) | LCM{LM(fi),LM(fj)},
   LCM{LM(fi),LM(fk)} != LCM{LM(fi),LM(fj)}, and
   LCM{LM(fi),LM(fj)} != LCM{LM(fj, fk)
  */

  // oldG is the Gbasis before the first line of f4buchbergerv is added
  // otherwise we might miss some new pairs to be added
  // f:=(1387482169552326*s*t1*t2^2-25694114250969*s*t1*t2+240071563017*s*t1+579168836143704*t1*t2^2-10725348817476*t1*t2+100212766488*t1):;fb:=(-7035747399*s*t1^2*t2^2+118865637*s*t1^2*t2-793881*s*t1^2+118865637*s*t1*t2^2-1167858*s*t1*t2+1944*s*t1-1089126*s*t2^2+1944*s*t2+18*s-2936742966*t1^2*t2^2+49601160*t1^2*t2-328050*t1^2+49601160*t1*t2^2-485514*t1*t2+972*t1-446148*t2^2+972*t2+36):;rmp:=s^2+10*s+4:;gbasis([f,fb,rmp],[s,t1,t2],revlex);
  template<class tdeg_t,class modint_t>
  void zgbasis_updatemod(vector<unsigned> & G,vector< paire > & B,const vectzpolymod<tdeg_t,modint_t> & res,unsigned pos,const vector<unsigned> & oldG,bool multimodular){
    if (debug_infolevel>2)
      CERR << CLOCK()*1e-6 << " zmod begin gbasis update " << G.size() << '\n';
    if (debug_infolevel>3)
      CERR << "G=" << G << "B=" << B << '\n';
    const zpolymod<tdeg_t,modint_t> & h = res[pos];
    order_t order=h.order;
    short dim=h.dim;
    vector<unsigned> C,Ccancel;
    C.reserve(G.size()+1);
    const tdeg_t & h0=h.ldeg;
    for (unsigned i=0;i<oldG.size();++i){
      if (tdeg_t_all_greater(h0,res[oldG[i]].ldeg,order))
	return;
    }
    tdeg_t tmp1,tmp2;
    // C is used to construct new pairs
    // create pairs with h and elements g of G, then remove
    // -> if g leading monomial is prime with h, remove the pair
    // -> if g leading monomial is not disjoint from h leading monomial
    //    keep it only if lcm of leading monomial is not divisible by another one
#if 1
    unsigned tmpsize = unsigned(G.size());
    vector<tdeg_t> tmp(tmpsize);
    for (unsigned i=0;i<tmpsize;++i){
      if (
#ifdef GIAC_GBASIS_REDUCTOR_MAXSIZE
	  res[G[i]].in_gbasis
#else
	  1
#endif
	  )
	index_lcm(h0,res[G[i]].ldeg,tmp[i],order);
      else
	tmp[i].tab[0]=-1;
    }
#else
    // this would be faster but it does not work for 
    // gbasis([25*y^2*x^6-10*y^2*x^5+59*y^2*x^4-20*y^2*x^3+43*y^2*x^2-10*y^2*x+9*y^2-80*y*x^6+136*y*x^5+56*y*x^4-240*y*x^3+104*y*x^2+64*x^6-192*x^5+192*x^4-64*x^3,25*y^2*6*x^5-10*y^2*5*x^4+59*y^2*4*x^3-20*y^2*3*x^2+43*y^2*2*x-10*y^2-80*y*6*x^5+136*y*5*x^4+56*y*4*x^3-240*y*3*x^2+104*y*2*x+64*6*x^5-192*5*x^4+192*4*x^3-64*3*x^2,25*2*y*x^6-10*2*y*x^5+59*2*y*x^4-20*2*y*x^3+43*2*y*x^2-10*2*y*x+9*2*y-80*x^6+136*x^5+56*x^4-240*x^3+104*x^2],[x,y],revlex);
    // pair <4,3> is not generated
    unsigned tmpsize=G.empty()?0:G.back()+1;
    vector<tdeg_t> tmp(tmpsize);
    for (unsigned i=0;i<tmpsize;++i){
      index_lcm(h0,res[i].ldeg,tmp[i],order); 
    }
#endif
    vector <tdeg_t> cancellables;
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
#ifdef GIAC_GBASIS_REDUCTOR_MAXSIZE
      if (!res[G[i]].in_gbasis)
	continue;
#endif
      if (interrupted || ctrl_c)
	return;
      if (disjoint(h0,res[G[i]].ldeg,order,dim)){
#ifdef GIAC_GBASIS_DELAYPAIRS
	Ccancel.push_back(G[i]);
#endif
	//cancellables.push_back(tmp[i]);
	continue;
      }
      //if (equalposcomp(cancellables,tmp[i])){ CERR << "cancelled!" << '\n'; continue; }
      // h0 and G[i] leading monomial not prime together
#if 1
      tdeg_t * tmp1=&tmp[i]; 
#else
      tdeg_t * tmp1=&tmp[G[i]]; 
#endif
      tdeg_t * tmp2=&tmp[0],*tmpend=tmp2+tmpsize;
      for (;tmp2!=tmp1;++tmp2){
	if (tmp2->tab[0]==-1)
	  continue;
	if (tdeg_t_all_greater(*tmp1,*tmp2,order))
	  break; // found another pair, keep the smallest, or the first if equal
      }
      if (tmp2!=tmp1){
	tmp1->tab[0]=-1; // desactivate tmp1 since it is >=tmp2
	continue;
      }
      for (++tmp2;tmp2<tmpend;++tmp2){
	if (tmp2->tab[0]==-1)
	  continue;
	if (tdeg_t_all_greater(*tmp1,*tmp2,order) && *tmp1!=*tmp2){
	  tmp1->tab[0]=-1; // desactivate tmp1 since it is >tmp2
	  break; 
	}
      }
      if (tmp2==tmpend)
	C.push_back(G[i]);
    }
    vector< paire > B1;
    B1.reserve(B.size()+C.size());
    for (unsigned i=0;i<B.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
      if (res[B[i].first].coord.empty() || res[B[i].second].coord.empty())
	continue;
      index_lcm_overwrite(res[B[i].first].ldeg,res[B[i].second].ldeg,tmp1,order);
#ifdef GIAC_GBASIS_DELAYPAIRS
      if (!multimodular){
	// look for the pair B[i].second,pos compared to B[i].first/B[i].second
	// and delay pair (seems slower in multimodular mode)
	// this speeds up problems with symmetries like cyclic*
	// comparisons below seem reversed but they increase speed probably
	// because they detect symmetries
	tdeg_t tmpBi2;
	index_lcm(h0,res[B[i].second].ldeg,tmpBi2,order);
	if (int p=equalposcomp(C,B[i].second)){
	  if (
	      0 && tmp1==tmpBi2
	      // tdeg_t_all_greater(tmp1,tmpBi2,order)
	      ){
	    C[p-1]=C[p-1]|0x80000000;
	    B1.push_back(B[i]);
	    continue;
	  }
	  if (
	      //tmp1==tmpBi2
	      tdeg_t_all_greater(tmpBi2,tmp1,order)
	      //tdeg_t_all_greater(tmp1,tmpBi2,order)
	      ){
	    // set B[i] for later reduction
	    B[i].live=false;
	    B1.push_back(B[i]);
	    continue;
	  }
	}
#if 1
	if (int p=equalposcomp(Ccancel,B[i].second)){
	  if (
	      //tmp1==tmpBi2
	      tdeg_t_all_greater(tmpBi2,tmp1,order)
	      //tdeg_t_all_greater(tmp1,tmpBi2,order)
             ){
	    B[i].live=false;
	    B1.push_back(B[i]);
	    continue;
	  }
	}
#endif
      }
#endif // GIAC_GBASIS_DELAYPAIRS
      if (!tdeg_t_all_greater(tmp1,h0,order)){
	B1.push_back(B[i]);
	continue;
      }
      index_lcm_overwrite(res[B[i].first].ldeg,h0,tmp2,order);
      if (tmp2==tmp1){
	B1.push_back(B[i]);
	continue;
      }
      index_lcm_overwrite(res[B[i].second].ldeg,h0,tmp2,order);
      if (tmp2==tmp1){
	B1.push_back(B[i]);
	continue;
      }
    }
    // B <- B1 union pairs(h,g) with g in C
    for (unsigned i=0;i<C.size();++i){
      B1.push_back(paire(pos,C[i]&0x7fffffff));
      if (C[i] & 0x80000000)
	B1.back().live=false;
    }
    swap(B1,B);
    // Update G by removing elements with leading monomial >= leading monomial of h
    if (debug_infolevel>2){
      CERR << CLOCK()*1e-6 << " end, pairs:"<< '\n';
      if (debug_infolevel>3)
	CERR << B << '\n';
      CERR << "mod begin Groebner interreduce " << '\n';
    }
    C.clear();
    C.reserve(G.size()+1);
    // bool pos_pushed=false;
    for (unsigned i=0;i<G.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c)
	return;
#ifdef GIAC_GBASIS_REDUCTOR_MAXSIZE
      if (!res[G[i]].in_gbasis)
	continue;
#endif
      if (!res[G[i]].coord.empty() && !tdeg_t_all_greater(res[G[i]].ldeg,h0,order)){
	C.push_back(G[i]);
	continue;
      }
#ifdef GIAC_GBASIS_REDUCTOR_MAXSIZE
      if (res[G[i]].coord.size()<=GIAC_GBASIS_REDUCTOR_MAXSIZE){
	res[G[i]].in_gbasis=false;
	C.push_back(G[i]);
      }
#endif
      // NB: removing all pairs containing i in it does not work
    }
    if (debug_infolevel>2)
      CERR << CLOCK()*1e-6 << " zmod end gbasis update " << '\n';
    for (unsigned i=0;i<C.size();++i){
      if (!res[C[i]].coord.empty() && tdeg_t_all_greater(h0,res[C[i]].ldeg,order)){
	swap(C,G); return;
      }
    }
    C.push_back(pos);
    swap(C,G);
  }

  template<class tdeg_t,class modint_t>
  void convert(const polymod<tdeg_t,modint_t> & p,zpolymod<tdeg_t,modint_t> & q,const vector<tdeg_t> & R){
    q.order=p.order;
    q.dim=p.dim;
    q.coord.clear();
    q.coord.reserve(p.coord.size());
    typename vector< T_unsigned<modint_t,tdeg_t> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    typename vector<tdeg_t>::const_iterator jt=R.begin(),jt0=jt,jtend=R.end();
    for (;it!=itend;++it){
      const tdeg_t & u=it->u;
      for (;jt!=jtend;++jt){
	if (*jt==u)
	  break;
      }
      if (jt!=jtend){
	q.coord.push_back( T_unsigned<modint_t,unsigned> (it->g,int(jt-jt0)));
	++jt;
      }
      else
	COUT << "not found" << '\n';
    }
    q.expo=&R;
    if (!q.coord.empty())
      q.ldeg=R[q.coord.front().u];
    q.fromleft=p.fromleft;
    q.fromright=p.fromright;
    q.age=p.age;
    q.logz=p.logz;
  }

  template<class tdeg_t,class modint_t>
  void convert(const zpolymod<tdeg_t,modint_t> & p,polymod<tdeg_t,modint_t> & q){
    q.dim=p.dim;
    q.order=p.order;
    q.coord.clear();
    q.coord.reserve(p.coord.size());
    typename vector<  T_unsigned<modint_t,unsigned>  >::const_iterator it=p.coord.begin(),itend=p.coord.end();
    const vector<tdeg_t> & expo=*p.expo;
    for (;it!=itend;++it){
      q.coord.push_back(T_unsigned<modint_t,tdeg_t>(it->g,expo[it->u]));
    }
    q.fromleft=p.fromleft;
    q.fromright=p.fromright;
    q.age=p.age;
    q.logz=p.logz;
  }

  template<class tdeg_t,class modint_t>
  void zincrease(vector<zpolymod<tdeg_t,modint_t> > &v){
    if (v.size()!=v.capacity())
      return;
    vector<zpolymod<tdeg_t,modint_t> > w;
    w.reserve(v.size()*2);
    for (unsigned i=0;i<v.size();++i){
      w.push_back(zpolymod<tdeg_t,modint_t>(v[i].order,v[i].dim,v[i].expo,v[i].ldeg));
      w[i].coord.swap(v[i].coord);
      w[i].age=v[i].age;
      w[i].fromleft=v[i].fromleft;
      w[i].fromright=v[i].fromright;
      w[i].age=v[i].age;
      w[i].logz=v[i].logz;
    }
    v.swap(w);
  }

  template<class tdeg_t,class modint_t>
  void smod(polymod<tdeg_t,modint_t> & resmod,modint_t env){
    typename std::vector< T_unsigned<modint_t,tdeg_t> >::iterator it=resmod.coord.begin(),itend=resmod.coord.end();
    for (;it!=itend;++it){
      modint_t n=it->g;
#ifdef GBASIS_4PRIMES
      n = smod(n,env);
#else
      if (n*2LL>env)
	it->g -= env;
      else {
	if (n*2LL<=-env)
	  it->g += env;
      }
#endif
    }
  }

  template<class tdeg_t,class modint_t>
  void smod(vectpolymod<tdeg_t,modint_t> & resmod,modint_t env){
    for (unsigned i=0;i<resmod.size();++i)
      smod(resmod[i],env);
  }

  template <class tdeg_t,class modint_t>
  double sumdegcoeffs2(const vector< vectpolymod<tdeg_t,modint_t> > * coeffsmodptr,const order_t & o,const vectzpolymod<tdeg_t,modint_t> &res,const paire & bk,int strategy){
    if (!coeffsmodptr) return 0;
    strategy %= 1000;
    int t1=res[bk.first].coord.size();
    int t2=res[bk.second].coord.size();
    if (t1==0 || t2==0)
      return 0;
    const tdeg_t & pi = res[bk.first].coord.front().u;
    const tdeg_t & qi = res[bk.second].coord.front().u;
    tdeg_t lcm;
    index_lcm(pi,qi,lcm,o);
    tdeg_t pshift=lcm-pi;
    tdeg_t qshift=lcm-qi;
    int N=(*coeffsmodptr)[bk.first].size();
    double T1=sumtermscoeffs((*coeffsmodptr)[bk.first]),T2=sumtermscoeffs((*coeffsmodptr)[bk.second]);
    double D1=sumdegcoeffs<tdeg_t,modint_t>((*coeffsmodptr)[bk.first],o),D2=sumdegcoeffs<tdeg_t,modint_t>((*coeffsmodptr)[bk.second],o);
    double d1=pshift.total_degree(o)+1,d2=qshift.total_degree(o)+1;
    if (strategy==17)
      return (N*t1+T1)*d1*D1+(N*t2+T2)*d2*D2;
    if (strategy==16)
      return t1*T1*d1+t2*T2*d2;
    if (strategy==15)
      return t1*T1*D1+t2*T2*D2;
    if (strategy==14)
      return (D1+N*d1)*T1+(D2+N*d2)*T2;
    if (strategy==13)
      return d1*T1+d2*T2;
    if (strategy==12)
      return D1*T1+D2*T2;
    if (strategy==11)
      return t1+t2;
    if (strategy==9)
      return N*t1+T1+N*t2+T2;
    if (strategy==8)
      return d1*(N*t1+T1)+d2*(N*t2+T2);
    if (strategy==10)
      return t1*T1*d1*D1+t2*T2*d2*D2;
    if (strategy==7)
      return (D1+N*d1)*(N*t1+T1)+(D2+N*d2)*(N*t2+T2);
    if (strategy==6)// || strategy==0) was default with topreduceonly=true
      return t1*T1+t2*T2;
    if (strategy==5)
      return D1*T1+D2*T2;
    // if (strategy==4)
      return t1*(D1+N*d1)*T1+t2*(D2+N*d2)*T2;
  }

  template<class tdeg_t,class modint_t>
  void reduce_syzygy(vector< vectpolymod<tdeg_t,modint_t> > & coeffs,const vectpolymod<tdeg_t,modint_t> & resmodorig,modint_t env){
    if (resmodorig.empty()) return;
    int dim=resmodorig[0].dim;
    order_t order=resmodorig[0].order;
    // try to reduce coeffs degrees using the identity f_i*f_j-f_j*f_i=0
    // assumes that the initial generator are sorted wrt the monomial order
    // resmod is the gbasis, let coeffs=*coeffsmodptr, f_j=resmodorig[j]
    // we have
    // resmod[k] = sum(coeffs[k][j]*f_j,j,0,resmod.size()-1)
    // f is sorted by decreasing order
    // if i<j then f_i<f_j for this order, let quo=coeffs[i] by f_j
    // and set coeffs[k][i] -= quo*f_j, coeffs[k][j] += quo*f_i
    int N=resmodorig.size(); // ==coeffs.size()
    polymod<tdeg_t,modint_t> TMP1;
    TMP1.dim=dim, TMP1.order=order;
    for (int k=0;k<coeffs.size();++k){
      vectpolymod<tdeg_t,modint_t> & coeffsk=coeffs[k];
      for (int i=N-2;i>=0;--i){
        // we will modify coeffsk[i]
        for (int j=i+1;j<N;++j){
          if (resmodorig[j].coord.front().u.total_degree(order)==resmodorig[i].coord.front().u.total_degree(order))
            continue;
          while (!coeffsk[i].coord.empty() && coeffsk[i].coord.front().u!=resmodorig[j].coord.front().u && tdeg_t_all_greater(coeffsk[i].coord.front().u,resmodorig[j].coord.front().u,order)){
            tdeg_t du(coeffsk[i].coord.front().u-resmodorig[j].coord.front().u);
            modint_t a(coeffsk[i].coord.front().g),b(resmodorig[j].coord.front().g),c(smod(a*extend(invmod(b,env)),env));
            smallmultsubmodshift(coeffsk[i],0,c,resmodorig[j],du,TMP1,env);
            coeffsk[i].swap(TMP1);
            smallmultsubmodshift(coeffsk[j],0,-c,resmodorig[i],du,TMP1,env);
            coeffsk[j].swap(TMP1);
          }
        }
      }
    }
  }

  template<class tdeg_t,class modint_t,class modint_t2>
  bool in_zgbasis(vectpolymod<tdeg_t,modint_t> &resmod,unsigned ressize,vector<unsigned> & G,modint_t env,bool totdeg,vector< paire > * pairs_reducing_to_zero,vector< zinfo_t<tdeg_t> > & f4buchberger_info,bool recomputeR,bool eliminate_flag,bool multimodular,int parallel,bool interred,const gbasis_param_t & gparam,vector< vectpolymod<tdeg_t,modint_t> > * coeffsmodptr){
    int strategy=gparam.buchberger_select_strategy;
    if (0 && multimodular && strategy>=0){ // safe multimodular strategies
      int s1=strategy/1000000,s2=(strategy/1000)%1000,s3=strategy%1000;
      if (s2==0 || s2==1 || s2==4) ; else s2=0;
      if (s3==999 || s3==1 || s3==2) ; else s3=0;
      strategy=s1*1000000+s2*1000+s3;
    }
    bool topreduceonly=strategy/1000000;
    vectpolymod<tdeg_t,modint_t> resmodorig(resmod); resmodorig.resize(ressize);
    unsigned generators=ressize;
    bool seldeg=true; int sel1=0;
    ulonglong cleared=0;
    unsigned learned_position=0,f4buchberger_info_position=0;
    bool learning=(coeffsmodptr && pairs_reducing_to_zero)?pairs_reducing_to_zero->empty():f4buchberger_info.empty();
    if (0 && learning && coeffsmodptr){ 
      // do a learning run with F4? 
      // requires pairs_reducing_to_zero to be the same (permutation...)
      // and comment multimodular after zf4mod call below
      vectpolymod<tdeg_t,modint_t> resmodcopy(resmod); vector<unsigned> Gcopy(G);
      in_zgbasis<tdeg_t,modint_t,modint_t2>(resmodcopy,ressize,Gcopy,env,totdeg,pairs_reducing_to_zero,f4buchberger_info,recomputeR,eliminate_flag,multimodular,parallel,interred, gparam,(vector< vectpolymod<tdeg_t,modint_t> > *) 0);
      learning=false;
    }
    unsigned capa = unsigned(f4buchberger_info.capacity());
    order_t order=resmod.front().order;
    short dim=resmod.front().dim;
    // if (order.dim-order.o==1) seldeg=false;
    polymod<tdeg_t,modint_t> TMP2(order,dim);
    vector< paire > B,BB;
    B.reserve(256); BB.reserve(256);
    vector<unsigned> smallposv;
    smallposv.reserve(256);
    info_t<tdeg_t,modint_t> information;
    if (order.o!=_REVLEX_ORDER && order.o!=_TDEG_ORDER)
      totdeg=false;
    vector<unsigned> oldG(G);
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " initial reduction: " << ressize << " memory " << memory_usage()*1e-6 << '\n';
    if (coeffsmodptr){ // initialize to "identity"
      coeffsmodptr->clear();
      coeffsmodptr->resize(ressize);
      tdeg_t dg(index_t(dim),order);
      TMP2.coord.push_back(T_unsigned<modint_t,tdeg_t>(create<modint_t>(1),dg));
      for (unsigned l=0;l<ressize;++l){
	(*coeffsmodptr)[l].resize(ressize);
        for (unsigned m=0;m<ressize;++m){
          (*coeffsmodptr)[l][m].order=order;
          (*coeffsmodptr)[l][m].dim=dim;
        }
	(*coeffsmodptr)[l][l]=TMP2;
      }
      TMP2.coord.clear();
    }
    for (unsigned l=0;l<ressize;++l){
#ifdef GIAC_REDUCEMODULO
      reducesmallmod(resmod[l],resmod,G,-1,env,TMP2,!is_zero(env),0,false,coeffsmodptr?&(*coeffsmodptr)[l]:0,coeffsmodptr);
      if (coeffsmodptr)
        reduceAF((*coeffsmodptr)[l],resmodorig,env,order);
#endif
      gbasis_updatemod(G,B,resmod,l,TMP2,env,coeffsmodptr?false:true,oldG);
    }
    for (unsigned l=0;l<ressize;++l){
      // debug
      if (0 && coeffsmodptr){ // check resmodorig=resmod at function begin
        polymod<tdeg_t,modint_t> _TMP1=resmod[l],_TMP2(order,dim);
        vector< vectpolymod<tdeg_t,modint_t> > & v = *coeffsmodptr;
        int s=v.front().size();
        vectpolymod<tdeg_t,modint_t> & newcoeffs=v[l];
        for (size_t k=0;k<s;k++){ // - sum(newcoeffs[k]*resmodorig[k])
          for (size_t l=0;l<newcoeffs[k].coord.size();++l){
            smallmultsubmodshift(_TMP1,0,newcoeffs[k].coord[l].g,resmodorig[k],newcoeffs[k].coord[l].u,_TMP2,env);
            _TMP1.coord.swap(_TMP2.coord);
          }
        }
        // should be 0
        if (_TMP1.coord.size())
          CERR << "zgbasis reduce coeff error " << _TMP1 << "\n";
      }
    }
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " initial collect, pairs " << B.size() << '\n';
    // init zpolymod<tdeg_t,modint> before main loop
    collect(resmod,TMP2);
    // R0 stores monomials for the initial basis
    vector<tdeg_t> R0(TMP2.coord.size());
    for (unsigned l=0;l<TMP2.coord.size();++l)
      R0[l]=TMP2.coord[l].u;
    // Rbuchberger stores monomials for pairs that are not handled by f4
    vector< vector<tdeg_t> > Rbuchberger;
    const int maxage=65535;
    Rbuchberger.reserve(maxage+1);
    vectzpolymod<tdeg_t,modint_t> res;
    res.resize(ressize);
    for (unsigned l=0;l<ressize;++l){
      convert(resmod[l],res[l],R0);
      zsmallmultmod(create<modint_t>(1),res[l],env);
    }
    resmod.clear();
    if (debug_infolevel>1000){
      res.dbgprint(); res[0].dbgprint(); // instantiate
    }
    double timebeg=CLOCK(),autodebug=5e8;
    vector<int> start_index_v;
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin loop, mem " << memory_usage()*1e-6 << '\n';
    int age;
    for (age=1;!B.empty() && !interrupted && !ctrl_c;++age){
      if (f4buchberger_info.size()>=capa-2 || age>maxage){
	CERR << "Error zgbasis too many iterations" << '\n';
	return false; // otherwise reallocation will make pointers invalid
      }
      if (debug_infolevel<2 && (CLOCK()-timebeg)>autodebug)
	debug_infolevel=multimodular?1:2;
      start_index_v.push_back(int(res.size())); // store size for final interreduction
#ifdef TIMEOUT
      control_c();
#endif
      if (f4buchberger_info_position>=capa-1){
	CERR << "Error f4 info exhausted" << '\n';
	return false;
      }
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " begin new iteration " << age << " zmod, " << env << " number of pairs: " << B.size() << ", base size: " << G.size() << '\n';
      vector<bool> clean(res.size(),true); 
      for (unsigned i=0;i<int(G.size());++i){
	clean[G[i]]=false;
      }
      if (!totdeg){
	for (unsigned i=0;i<int(res.size());++i){
	  if (res[i].maxtdeg==-1)
	    res[i].compute_maxtdeg();
	}
      }
      vector<tdeg_t> Blcm(B.size());
      vector<int> Blcmdeg(B.size()),Blogz(B.size());
      vector<unsigned> nterms(B.size());
      for (unsigned i=0;i<B.size();++i){
	clean[B[i].first]=false;
	clean[B[i].second]=false;
        Blogz[i]=res[B[i].first].logz+res[B[i].second].logz;
	index_lcm(res[B[i].first].ldeg,res[B[i].second].ldeg,Blcm[i],order);
	if (!totdeg)
	  Blcmdeg[i]=giacmax(res[B[i].first].maxtdeg+(Blcm[i]-res[B[i].first].ldeg).total_degree(order),res[B[i].second].maxtdeg+(Blcm[i]-res[B[i].second].ldeg).total_degree(order));
	nterms[i]=unsigned(res[B[i].first].coord.size()+res[B[i].second].coord.size());
      }
#if 1
      for (unsigned i=0;i<clean.size();++i){
	if (clean[i] && res[i].coord.capacity()>1){
	  cleared += int(res[i].coord.capacity())-1;
	  zpolymod<tdeg_t,modint_t> clearer;
	  clearer.coord.swap(res[i].coord);
	}
      }
#endif
      vector< paire > smallposp;
      vector<unsigned> smallposv;
      if (!totdeg){
	// find smallest lcm pair in B
	// could also take nterms[i] in account
	unsigned firstdeg=RAND_MAX-1;
	for (unsigned i=0;i<B.size();++i){
	  if (!B[i].live) continue;
	  unsigned f=seldeg?Blcmdeg[i]:Blcm[i].selection_degree(order);
	  //unsigned f=Blcmdeg[i]; 
	  if (f>firstdeg)
	    continue;
	  if (f<firstdeg){
	    firstdeg=f;
	    continue;
	  }
	}
	for (unsigned i=0;i<B.size();++i){
	  if (!B[i].live) continue;
	  if (
	      (seldeg?Blcmdeg[i]:Blcm[i].selection_degree(order))==firstdeg
	      //Blcmdeg[i]==firstdeg
	      ){
	    smallposv.push_back(i);
	  }
	}
	if (smallposv.empty()) smallposv.resize(B.size());
	if (debug_infolevel>1)
	  CERR << CLOCK()*1e-6 << " zpairs min " << (seldeg?"total degree ":"elimination degree ") << firstdeg << " #pairs " << smallposv.size() << '\n';
	if ( seldeg && (smallposv.size()<giacmin(order.o,3))
	    ){
	  ++sel1;
	  if (sel1%5==0){
	    seldeg=!seldeg;
	    if (debug_infolevel)
	      CERR << "Switching pair selection strategy to " << (seldeg?"total degree":"elimination degree") << '\n';
	  }
	}
	else
	  sel1=0;
	if (int(firstdeg)>GBASISF4_MAX_TOTALDEG){
	  CERR << "Error zgbasis degree too large" << '\n';
	  return false;
	}
      }
      else {
	// find smallest lcm pair in B
	unsigned smallnterms=RAND_MAX,firstdeg=RAND_MAX-1,ismallnterms=-1;
	for (unsigned i=0;i<B.size();++i){
	  if (!B[i].live) continue;
	  unsigned f=Blcm[i].total_degree(order);
	  if (f>firstdeg)
	    continue;
	  if (f<firstdeg){
	    firstdeg=f;
	    smallnterms=nterms[i];
            ismallnterms=i;
	    continue;
	  }
	  if (nterms[i]<smallnterms){
	    smallnterms=nterms[i];
            ismallnterms=i;
          }
	}
        if (0 && coeffsmodptr) // disabled, smallest number of terms is slower than just first pair in cyclic6 with coeffs
          smallposv.push_back(ismallnterms);
        else {
          smallnterms *= 5;
          for (unsigned i=0;i<B.size();++i){
            if (!B[i].live) continue;
            if (
                //nterms[i]<=smallnterms && 
                Blcm[i].total_degree(order)==firstdeg){
              smallposv.push_back(i);
            }
          }
        }
	if (smallposv.empty()) smallposv.resize(B.size());
	if (debug_infolevel>1)
	  CERR << CLOCK()*1e-6 << " zpairs min total degrees, nterms " << firstdeg << "," << smallnterms << " #pairs " << smallposv.size() << '\n';
      }
      if (debug_infolevel>3)
	CERR << "pairs reduced " << B << " indices " << smallposv << '\n';
#if 1
      // note that this is too slow for I0:=[2*v7-v3-v1,2*v8-v4-v2,2*v9-v5-v1,2*v10-v6-v2,-v12+v10-v5+v1,-v11+v9+v6-v2,-v14+v8+v3-v1,-v13+v7-v4+v2,v15*v12-v16*v11-v15*v10+v11*v10+v16*v9-v12*v9,v15*v14-v16*v13-v15*v8+v13*v8+v16*v7-v14*v7,v17*v14-v18*v13-v17*v8+v13*v8+v18*v7-v14*v7,-v18^2-v17^2+2*v18*v16+2*v17*v15-2*v16*v2+v2^2-2*v15*v1+v1^2,v19*v12-v20*v11-v19*v10+v11*v10+v20*v9-v12*v9,-v20^2-v19^2+2*v20*v16+2*v19*v15-2*v16*v2+v2^2-2*v15*v1+v1^2,-v21*v4+v22*v3+v21*v2-v3*v2-v22*v1+v4*v1,v21*v20-v22*v19-v21*v18+v19*v18+v22*v17-v20*v17,v23*v6-v24*v5-v23*v2+v5*v2+v24*v1-v6*v1,v23*v20-v24*v19-v23*v18+v19*v18+v24*v17-v20*v17,-1+v27*v24^2+v27*v23^2-v27*v22^2-v27*v21^2-2*v27*v24*v2+2*v27*v22*v2-2*v27*v23*v1+2*v27*v21*v1,-1+v28*v6^2-2*v28*v6^3+v28*v6^4+v28*v5^2-2*v28*v6*v5^2+2*v28*v6^2*v5^2+v28*v5^4]:;I1:=subst(I0,[v4=1,v3=0,v2=0,v1=0]):;v:=[v7,v8,v9,v10,v11,v12,v13,v14,v15,v16,v17,v18,v19,v20,v21,v22,v23,v24,v27,v28,v1,v2,v3,v4,v5,v6]:;G,M:=gbasis(I1,v,coeffs):;
      vector< paire > coeffszeropairs;
      const vector<unsigned> * coeffpermuBptr=0;
      bool usef4=false;
      //usef4=true;
      // dry run with F4 is not optimal, probably because it discards
      // some pairs that would be nice reducers instead of other
      if (// 1 ||  // FIXME comment 1 ||
          (strategy % 1000 ==2 || strategy % 1000 ==99) && 
          (coeffsmodptr || (order.o!=_REVLEX_ORDER && smallposv.size()<=GBASISF4_BUCHBERGER) )
          ){
        int Rbuchbergersize=Rbuchberger.size();
        vector<unsigned> oldG(G);
	// pairs not handled by f4
	int modsize=int(resmod.size());
	if (modsize<res.size())
	  resmod.resize(res.size());
	for (int i=0;i<int(G.size());++i){
	  int Gi=G[i];
	  if (resmod[Gi].coord.empty())
	    convert(res[Gi],resmod[Gi]);
	}
	polymod<tdeg_t,modint_t> TMP1(order,dim),TMP2(order,dim);
	zpolymod<tdeg_t,modint_t> TMP;
	paire bk;
        int np=smallposv.size(); // number of s-pairs
        if (coeffsmodptr){
          if (strategy%1000==99){
            vector< pair<int,tdeg_t> > V(np);
            for (int count=0;count<np;++count){
              V[count].first=count;
              V[count].second=Blcm[smallposv[count]];
            }
            sort(V.begin(),V.end(),tdeg_t_sort_t<tdeg_t>(order));
            reverse(V.begin(),V.end());
            for (int count=0;count<np;++count){
              int pos=smallposv[V[count].first];
              smallposp.push_back(B[pos]);            
            }
            for (int i=int(np)-1;i>=0;--i)
              B.erase(B.begin()+smallposv[i]);
          }
          else {
            // sort pairs ?
            vector< pair<int,double> > V(np);
            for (int count=0;count<np;++count){
              V[count].first=count;
              V[count].second=sumdegcoeffs2(coeffsmodptr,order,res,B[smallposv[count]],0);
            }
            sort(V.begin(),V.end(),tripair);
            double logV0=GBASIS_COEFF_MAXLOGRATIO*std::log(V.front().second);
            smallposp.clear();
            vector<int> toremove;
            for (int count=0;count<np;++count){
              double curlog=std::log(V[count].second);
              if (count && curlog>=logV0)
                break;
              int pos=smallposv[V[count].first];
              smallposp.push_back(B[pos]);
              toremove.push_back(pos);
            }
            sort(toremove.begin(),toremove.end());
            if (debug_infolevel>1)
              CERR << CLOCK()*1e-6 << "Reducing " << toremove.size() << " pairs, from " << np << " pairs of minimal degree\n";
            // remove selected pairs from B
            for (int i=int(toremove.size())-1;i>=0;--i)
              B.erase(B.begin()+toremove[i]);
          }
        }
        else {
          smallposp.clear();
          for (int count=0;count<np;++count){
            smallposp.push_back(B[smallposv[count]]);
          }
          // remove selected pairs from B
          for (int i=int(np)-1;i>=0;--i)
            B.erase(B.begin()+smallposv[i]);
        }
        if (usef4 && coeffsmodptr && (learning || !multimodular)){
          // make a "dry" F4 run, not computing coefficients
          // and update pairs_reducing_to_zero
          vectzpolymod<tdeg_t,modint_t> new_res(res);
          vectzpolymod<tdeg_t,modint_t> f4buchbergerv; // collect all spolys
          unsigned int coeffs_learned_position(learned_position);
          int f4res=-1;
          f4res=zf4mod<tdeg_t,modint_t,modint_t2>(new_res,G,env,smallposp,coeffpermuBptr,f4buchbergerv,true /* learning*/,coeffs_learned_position,&coeffszeropairs,f4buchberger_info,f4buchberger_info_position,recomputeR,age,multimodular,parallel,0);
          if (f4res==-1)
            return false;
          if (coeffpermuBptr){
            if (debug_infolevel>1)
              CERR << "learning f4buchberger [" ;
            for (unsigned i=0;i<f4buchbergerv.size();++i){
              if (f4buchbergerv[i].coord.empty()){
                if (debug_infolevel>1)
                  CERR << smallposp[(*coeffpermuBptr)[i]] << ',';
                coeffszeropairs.push_back(smallposp[(*coeffpermuBptr)[i]]);
              }
            }
            if (debug_infolevel>1)
              CERR << "]\n"; 
            sort(coeffszeropairs.begin(),coeffszeropairs.end());
            // sort pairs using remsize
            int S=f4buchbergerv.size();
            vector< pair<unsigned,unsigned> > usef4v(S);
            for (int i=0;i<S;++i){
              usef4v[i]=pair<unsigned,unsigned>(f4buchbergerv[i].coord.size(),(*coeffpermuBptr)[i]);
            }
            sort(usef4v.begin(),usef4v.end());
            vector<paire> P(smallposp); smallposp.clear();
            for (int i=0;i<S;++i)
              smallposp.push_back(P[usef4v[i].second]);
          }
        } // end usef4
        
        for (int count=0;count<smallposp.size();++count){
          bk=smallposp[count];//coeffpermuBptr?smallposp[(*coeffpermuBptr)[count]]:smallposp[count];
          if (coeffsmodptr && !coeffszeropairs.empty() && binary_search(coeffszeropairs.begin(),coeffszeropairs.end(),bk)){
            if (learning && pairs_reducing_to_zero){
              if (debug_infolevel>1)
                CERR << "learning from dry prerun " << bk << '\n';
              pairs_reducing_to_zero->push_back(bk);
            }
            continue;
          }
          if (!learning  && pairs_reducing_to_zero && learned_position<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[learned_position]){
            if (debug_infolevel>2)
              CERR << bk << " learned " << learned_position << '\n';
            ++learned_position;
            continue;
          }
          if (debug_infolevel>2)
            CERR << bk << " not learned " << learned_position << '\n';
          if (resmod[bk.first].coord.empty())
            convert(res[bk.first],resmod[bk.first]);
          if (resmod[bk.second].coord.empty())
            convert(res[bk.second],resmod[bk.second]);
          modint_t d=spolymod<tdeg_t,modint_t>(resmod[bk.first],resmod[bk.second],TMP1,TMP2,env);
          if (!usef4 && coeffsmodptr){ 
            if (learning || !multimodular){
              polymod<tdeg_t,modint_t> TMP3(TMP1);
              if (debug_infolevel>1)
                CERR << CLOCK()*1e-6 << " to ";
              reducesmallmod(TMP3,resmod,G,-1,env,TMP2,true,0,true);
              if (debug_infolevel>1)
                CERR << CLOCK()*1e-6 << " dry reduction " << bk << " remsize=" << TMP3.coord.size() << " pairs " << B.size() << " basis " << G.size() << "\n";
              if (TMP3.coord.empty()){
                if (learning && pairs_reducing_to_zero){
                  if (debug_infolevel>2)
                    CERR << "learning " << bk << '\n';
                  pairs_reducing_to_zero->push_back(bk);
                }
                continue;
              }
            }
          }
          vectpolymod<tdeg_t,modint_t> newcoeffs;
          if (coeffsmodptr){
            vector< vectpolymod<tdeg_t,modint_t> > & v = *coeffsmodptr;
            int s=v.front().size();
            newcoeffs.resize(s);
            int i1=bk.first,i2=bk.second;
            const polymod<tdeg_t,modint_t> &p=resmod[i1];
            const polymod<tdeg_t,modint_t> &q=resmod[i2];
            if (p.coord.empty() || q.coord.empty())
              return false;
            modint_t a=p.coord.front().g,b=q.coord.front().g;
            modint_t c=smod(extend(a)*invmod(b,env),env);
            const tdeg_t & pi = p.coord.front().u;
            const tdeg_t & qi = q.coord.front().u;
            tdeg_t lcm;
            index_lcm(pi,qi,lcm,p.order);
            tdeg_t pshift=lcm-pi;
            tdeg_t qshift=lcm-qi;
            polymod<tdeg_t,modint_t> _TMP1(order,dim),_TMP2(order,dim),_TMP3(order,dim);
            vectpolymod<tdeg_t,modint_t> & curfirst=v[i1];
            vectpolymod<tdeg_t,modint_t> & cursecond=v[i2];
            for (size_t k=0;k<s;k++){
              _TMP1=curfirst[k];
              _TMP2=cursecond[k];
              smallshift(_TMP1.coord,pshift,_TMP1.coord);
              smallmultsubmodshift(_TMP1,0,c,_TMP2,qshift,_TMP3,env);
              smallmultmod(d,_TMP3,env);
              newcoeffs[k]=_TMP3;
            }
            // debug
            if (0){ // check resmodorig=resmod at function begin
              _TMP1=TMP1; 
              for (size_t k=0;k<s;k++){ // - sum(newcoeffs[k]*resmodorig[k])
                for (size_t l=0;l<newcoeffs[k].coord.size();++l){
                  smallmultsubmodshift(_TMP1,0,newcoeffs[k].coord[l].g,resmodorig[k],newcoeffs[k].coord[l].u,_TMP2,env);
                  _TMP1.coord.swap(_TMP2.coord);
                }
              }
              // should be 0
              if (_TMP1.coord.size())
                CERR << "zgbasis spoly coeff error " << _TMP1 << "\n";
            }
          }
          if (debug_infolevel>2){
            CERR << CLOCK()*1e-6 << " mod reduce begin, pair " << bk << " spoly size " << TMP1.coord.size() << " totdeg deg " << TMP1.coord.front().u.total_degree(order) << " degree " << TMP1.coord.front().u << ", pair degree " << resmod[bk.first].coord.front().u << resmod[bk.second].coord.front().u << '\n';
          }
          reducesmallmod(TMP1,resmod,G,-1,env,TMP2,true,0,topreduceonly,&newcoeffs,coeffsmodptr,strategy); // strategy might be modified to usemap=true if previous reducesmallmod returned a large size remainder (in that case it is expected that the computation is large)
          // insure that new basis element has positive coord, required by zf4mod
          typename vector< T_unsigned<modint_t,tdeg_t> >::iterator it=TMP1.coord.begin(),itend=TMP1.coord.end();
          for (;it!=itend;++it){
            // if (it->g<0) it->g += env;
            it->g += ((it->g>>31)&env);
          }
          // reducemod(TMP1,resmod,G,-1,TMP1,env,true);
          if (debug_infolevel>3){
            if (debug_infolevel>4){ CERR << TMP1 << '\n'; }
            CERR << CLOCK()*1e-6 << " mod reduce end, remainder degree " << (TMP1.coord.empty()?0:TMP1.coord.front().u) << " size " << TMP1.coord.size() << " begin gbasis update" << '\n';
          }
          if (!TMP1.coord.empty()){
            resmod.push_back(TMP1);
            reduceAF(newcoeffs,resmodorig,env,order);
            if (coeffsmodptr){
              coeffsmodptr->push_back(newcoeffs);
              // if coeffsmodptr, we need TMP and res only to run zgbasis_updatemod, maybe we could run gbasis_updatemod without zpolymod with reduce=false argument
            }
            Rbuchberger.push_back(vector<tdeg_t>(TMP1.coord.size()));
            vector<tdeg_t> & R0=Rbuchberger.back();
            for (unsigned l=0;l<unsigned(TMP1.coord.size());++l)
              R0[l]=TMP1.coord[l].u;
            convert(TMP1,TMP,R0);
            zincrease(res);
            if (ressize==res.size())
              res.push_back(zpolymod<tdeg_t,modint_t>(order,dim,TMP.ldeg));
            res[ressize].expo=TMP.expo;
            res[ressize].age=age;
            res[ressize].logz=res[bk.first].logz+res[bk.second].logz;
            swap(res[ressize].coord,TMP.coord);
            ++ressize;
            zgbasis_updatemod(G,B,res,ressize-1,oldG,multimodular);
            if (debug_infolevel>4)
              CERR << CLOCK()*1e-6 << " mod basis indexes " << G << " pairs indexes " << B << '\n';
          }
          else {
            if (learning && pairs_reducing_to_zero){
              if (debug_infolevel>2)
                CERR << "learning " << bk << '\n';
              pairs_reducing_to_zero->push_back(bk);
            }
          }
        } // end for loop on all spairs
        if (1 && coeffsmodptr){
          if (0 && usef4){
            // upper interreduction
            for (int i=G.size()-2;i>=0;--i){
              reducesmallmod(resmod[G[i]],resmod,G,i,env,TMP2,true,0,false,coeffsmodptr?&(*coeffsmodptr)[G[i]]:0,coeffsmodptr,strategy); // strategy==2
            }
          }
          // update res: keep only 1 monomial pointee
          if (Rbuchberger.size()>Rbuchbergersize+1){
            TMP2.coord.clear();
            collect(resmod,TMP2);
            Rbuchberger.resize(Rbuchbergersize);
            Rbuchberger.push_back(vector<tdeg_t>(TMP2.coord.size()));
            vector<tdeg_t> & R0=Rbuchberger.back();
            for (unsigned l=0;l<unsigned(TMP2.coord.size());++l)
              R0[l]=TMP2.coord[l].u;
            for (int i=0;i<resmod.size();++i){
              // doing that for G[i] is not enough, there are pairs with first or second elements not in G
              convert(resmod[i],res[i],R0);
            }
          }
        }
        continue;
      } // end strategy%1000==2 && coeffsmodptr or smallposp.size() small (<=GBASISF4_BUCHBERGER)
#endif
      if (// 1 ||  // FIXME comment 1 || 
	  coeffsmodptr || (order.o!=_REVLEX_ORDER && smallposv.size()<=GBASISF4_BUCHBERGER) || (strategy%1000)){ 
	// pairs not handled by f4
	int modsize=int(resmod.size());
	if (modsize<res.size())
	  resmod.resize(res.size());
	for (int i=0;i<int(G.size());++i){
	  int Gi=G[i];
	  if (resmod[Gi].coord.empty())
	    convert(res[Gi],resmod[Gi]);
	}
	polymod<tdeg_t,modint_t> TMP1(order,dim),TMP2(order,dim);
	zpolymod<tdeg_t,modint_t> TMP;
	paire bk; int curlogz=1; double sumdeg=0; 
        if (coeffsmodptr){
          if (strategy % 1000==999){
            int lcmpos=smallposv[0]; 
            tdeg_t deg(Blcm[lcmpos]);
            for (int i=1;i<smallposv.size();++i){
              int cur=smallposv[i];
              if (tdeg_t_greater(deg,Blcm[cur],order)){
                lcmpos=i;
                deg=Blcm[cur];
              }
            }
            bk=B[lcmpos];
            B.erase(B.begin()+lcmpos);
            curlogz=Blogz[lcmpos];
          }
          else if (strategy % 1000==1){
            bk=B[smallposv.front()]; B.erase(B.begin()+smallposv.front());
            curlogz=Blogz[smallposv.front()];
          }  else {
            // find best(?) pair
            int sumdegpos=0; sumdeg=sumdegcoeffs2<tdeg_t,modint_t>(coeffsmodptr,order,res,B[smallposv.front()],strategy);
            for (int i=1;i<smallposv.size();++i){
              double cur=sumdegcoeffs2<tdeg_t,modint_t>(coeffsmodptr,order,res,B[smallposv[i]],strategy);
              if (cur<sumdeg){
                sumdegpos=i;
                sumdeg=cur;
              }
            }
            bk=B[smallposv[sumdegpos]]; B.erase(B.begin()+smallposv[sumdegpos]);
            curlogz=Blogz[smallposv[sumdegpos]];
          }
        }
        else {
          bk=B[smallposv.back()];
          curlogz=Blogz[smallposv.back()];
          B.erase(B.begin()+smallposv.back());
        }
        if (debug_infolevel>=2)
          CERR << CLOCK()*1e-6 << " cur pair " << bk << ",logz=" << curlogz << ", deg/terms=" << sumdeg << " (strategy=" << strategy << ")\n";
	if (!learning && pairs_reducing_to_zero && learned_position<pairs_reducing_to_zero->size() && bk==(*pairs_reducing_to_zero)[learned_position]){
	  if (debug_infolevel>2)
	    CERR << bk << " learned " << learned_position << '\n';
	  ++learned_position;
	  continue;
	}
	if (debug_infolevel>2)
	  CERR << bk << " not learned " << learned_position << '\n';
	if (resmod[bk.first].coord.empty())
	  convert(res[bk.first],resmod[bk.first]);
	if (resmod[bk.second].coord.empty())
	  convert(res[bk.second],resmod[bk.second]);
	modint_t d=spolymod<tdeg_t,modint_t>(resmod[bk.first],resmod[bk.second],TMP1,TMP2,env);
	vectpolymod<tdeg_t,modint_t> newcoeffs;
	if (coeffsmodptr){ 
          if (learning || !multimodular){
            polymod<tdeg_t,modint_t> TMP3(TMP1);
            reducesmallmod(TMP3,resmod,G,-1,env,TMP2,true,0,true);
            if (debug_infolevel>=2)
              CERR << CLOCK()*1e-6 << " dry reduction " << bk << " remsize=" << TMP3.coord.size() << "\n";
            if (TMP3.coord.empty()){
              if (learning && pairs_reducing_to_zero){
                if (debug_infolevel>2)
                  CERR << "learning " << bk << '\n';
                pairs_reducing_to_zero->push_back(bk);
              }
              continue;
            }
          }
	  vector< vectpolymod<tdeg_t,modint_t> > & v = *coeffsmodptr;
	  int s=v.front().size();
	  newcoeffs.resize(s);
	  int i1=bk.first,i2=bk.second;
	  const polymod<tdeg_t,modint_t> &p=resmod[i1];
	  const polymod<tdeg_t,modint_t> &q=resmod[i2];
	  if (p.coord.empty() || q.coord.empty())
	    return false;
	  modint_t a=p.coord.front().g,b=q.coord.front().g;
	  modint_t c=smod(extend(a)*invmod(b,env),env);
	  const tdeg_t & pi = p.coord.front().u;
	  const tdeg_t & qi = q.coord.front().u;
	  tdeg_t lcm;
	  index_lcm(pi,qi,lcm,p.order);
	  tdeg_t pshift=lcm-pi;
	  tdeg_t qshift=lcm-qi;
	  polymod<tdeg_t,modint_t> _TMP1(order,dim),_TMP2(order,dim),_TMP3(order,dim);
          vectpolymod<tdeg_t,modint_t> & curfirst=v[i1];
          vectpolymod<tdeg_t,modint_t> & cursecond=v[i2];
	  for (size_t k=0;k<s;k++){
	    _TMP1=curfirst[k];
	    _TMP2=cursecond[k];
	    smallshift(_TMP1.coord,pshift,_TMP1.coord);
	    smallmultsubmodshift(_TMP1,0,c,_TMP2,qshift,_TMP3,env);
	    smallmultmod(d,_TMP3,env);
	    newcoeffs[k]=_TMP3;
	  }
          // debug
          if (0){ // check resmodorig=resmod at function begin
            _TMP1=TMP1; 
            for (size_t k=0;k<s;k++){ // - sum(newcoeffs[k]*resmodorig[k])
              for (size_t l=0;l<newcoeffs[k].coord.size();++l){
                smallmultsubmodshift(_TMP1,0,newcoeffs[k].coord[l].g,resmodorig[k],newcoeffs[k].coord[l].u,_TMP2,env);
                _TMP1.coord.swap(_TMP2.coord);
              }
            }
            // should be 0
            if (_TMP1.coord.size())
              CERR << "zgbasis spoly coeff error " << _TMP1 << "\n";
          }
	} // end coeffsmodptr
	if (debug_infolevel>2){
	  CERR << CLOCK()*1e-6 << " mod reduce begin, pair " << bk << " spoly size " << TMP1.coord.size() << " totdeg deg " << TMP1.coord.front().u.total_degree(order) << " degree " << TMP1.coord.front().u << ", pair degree " << resmod[bk.first].coord.front().u << resmod[bk.second].coord.front().u << '\n';
	}
	reducesmallmod(TMP1,resmod,G,-1,env,TMP2,true /* normalize */,0/* start index*/,topreduceonly,&newcoeffs,coeffsmodptr,strategy);
	// insure that new basis element has positive coord, required by zf4mod
	typename vector< T_unsigned<modint_t,tdeg_t> >::iterator it=TMP1.coord.begin(),itend=TMP1.coord.end();
	for (;it!=itend;++it){
	  // if (it->g<0) it->g += env;
          it->g += ((it->g>>31)&env);
	}
	// reducemod(TMP1,resmod,G,-1,TMP1,env,true);
	if (debug_infolevel>2){
	  if (debug_infolevel>3){ CERR << TMP1 << '\n'; }
	  CERR << CLOCK()*1e-6 << " mod reduce end, remainder degree " << (TMP1.coord.empty()?0:TMP1.coord.front().u) << " size " << TMP1.coord.size() << " begin gbasis update" << '\n';
	}
	if (!TMP1.coord.empty()){
	  resmod.push_back(TMP1);
          reduceAF(newcoeffs,resmodorig,env,order);
	  if (coeffsmodptr){
	    coeffsmodptr->push_back(newcoeffs);
	    // if coeffsmodptr, we need TMP and res only to run zgbasis_updatemod, maybe we could run gbasis_updatemod without zpolymod with reduce=false argument
            // if (!gparam.rawcoeffs) reduce_syzygy(*coeffsmodptr,resmodorig,env);
	  }
	  Rbuchberger.push_back(vector<tdeg_t>(TMP1.coord.size()));
	  vector<tdeg_t> & R0=Rbuchberger.back();
	  for (unsigned l=0;l<unsigned(TMP1.coord.size());++l)
	    R0[l]=TMP1.coord[l].u;
	  convert(TMP1,TMP,R0);
	  zincrease(res);
	  if (ressize==res.size())
	    res.push_back(zpolymod<tdeg_t,modint_t>(order,dim,TMP.ldeg));
	  res[ressize].expo=TMP.expo;
          res[ressize].fromleft=bk.first;
          res[ressize].fromright=bk.second;
          res[ressize].logz=curlogz;
          res[ressize].age=age;
	  swap(res[ressize].coord,TMP.coord);
	  ++ressize;
	  zgbasis_updatemod(G,B,res,ressize-1,G,multimodular);
	  if (debug_infolevel>3)
	    CERR << CLOCK()*1e-6 << " mod basis indexes " << G << " pairs indexes " << B << '\n';
	}
	else {
	  if (learning && pairs_reducing_to_zero){
	    if (debug_infolevel>2)
	      CERR << "learning " << bk << '\n';
	    pairs_reducing_to_zero->push_back(bk);
	  }
	}
	continue;
      } // end if smallposp.size() small (<=GBASISF4_BUCHBERGER)
      unsigned np=smallposv.size();
      if (np==B.size() && np<=max_pairs_by_iteration){
	swap(smallposp,B);
	B.clear();
      }
      else {
	// multiply by parallel?
	if (//!pairs_reducing_to_zero && 
	    np>max_pairs_by_iteration) 
	  np=max_pairs_by_iteration;
	for (unsigned i=0;i<np;++i)
	  smallposp.push_back(B[smallposv[i]]);
	// remove pairs
	for (int i=int(np)-1;i>=0;--i)
	  B.erase(B.begin()+smallposv[i]);
      }
      vectzpolymod<tdeg_t,modint_t> f4buchbergerv; // collect all spolys
      int f4res=-1;
      const vector<unsigned> * permuBptr=0;
#if 0 
      unsigned Galls=G.back();
      vector<unsigned> Gall;
      Gall.reserve(Galls);
      for (unsigned i=0;i<Galls;++i){
	if (!clean[i])
	  Gall.push_back(i);
      }
      f4res=zf4mod<tdeg_t,modint_t,modint_t2>(res,Gall,env,smallposp,permuBptr,f4buchbergerv,learning,learned_position,pairs_reducing_to_zero,f4buchberger_info,f4buchberger_info_position,recomputeR,age,multimodular,parallel,0);
#else
      f4res=zf4mod<tdeg_t,modint_t,modint_t2>(res,G,env,smallposp,permuBptr,f4buchbergerv,learning,learned_position,pairs_reducing_to_zero,f4buchberger_info,f4buchberger_info_position,recomputeR,age,multimodular,parallel,0);
#endif
      if (f4res==-1)
	return false;
      if (f4res==0)
	continue;
      if (!permuBptr && !learning && f4buchberger_info_position-1<f4buchberger_info.size())
	permuBptr=&f4buchberger_info[f4buchberger_info_position-1].permuB;
      if (permuBptr){
	for (unsigned i=0;i<f4buchbergerv.size();++i){
	  f4buchbergerv[i].fromleft=smallposp[(*permuBptr)[i]].first;
	  f4buchbergerv[i].fromright=smallposp[(*permuBptr)[i]].second;
	  f4buchbergerv[i].logz=res[f4buchbergerv[i].fromleft].logz+res[f4buchbergerv[i].fromright].logz;
	}
      }
      // update gbasis and learning
      // requires that Gauss pivoting does the same permutation for other primes
      if (multimodular && 
	  learning && pairs_reducing_to_zero){
	for (unsigned i=0;i<f4buchbergerv.size();++i){
	  if (f4buchbergerv[i].coord.empty()){
	    if (debug_infolevel>2)
	      CERR << "learning f4buchberger " << smallposp[(*permuBptr)[i]] << '\n';
	    pairs_reducing_to_zero->push_back(smallposp[(*permuBptr)[i]]);
	  }
	}
      }
      unsigned added=0;
      for (unsigned i=0;i<f4buchbergerv.size();++i){
	if (!f4buchbergerv[i].coord.empty())
	  ++added;
      }
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " reduce f4buchberger end on " << added << " from " << f4buchbergerv.size() << " pairs, zgbasis update begin" << '\n';
      vector<unsigned> oldG(G);
      for (int i=0;i<f4buchbergerv.size();++i){
	// for (int i=f4buchbergerv.size()-1;i>=0;--i){
	if (!f4buchbergerv[i].coord.empty()){
	  zincrease(res);
	  if (debug_infolevel>2)
	    CERR << CLOCK()*1e-6 << " adding to basis leading degree " << f4buchbergerv[i].ldeg << '\n';
	  if (ressize==res.size())
	    res.push_back(zpolymod<tdeg_t,modint_t>(order,dim,f4buchbergerv[i].ldeg));
	  res[ressize].expo=f4buchbergerv[i].expo;
	  swap(res[ressize].coord,f4buchbergerv[i].coord);
	  res[ressize].age=f4buchbergerv[i].age;
	  res[ressize].fromleft=f4buchbergerv[i].fromleft;
	  res[ressize].fromright=f4buchbergerv[i].fromright;
	  res[ressize].logz=f4buchbergerv[i].logz;
	  ++ressize;
	  if (!multimodular || learning || f4buchberger_info_position-1>=f4buchberger_info.size())
	    zgbasis_updatemod(G,B,res,ressize-1,oldG,multimodular);
	}
	else {
	  // if (!learning && pairs_reducing_to_zero)  CERR << " error learning "<< '\n';
	}
      }
      if (!multimodular) continue;
      if (!learning && f4buchberger_info_position-1<f4buchberger_info.size()){
	B=f4buchberger_info[f4buchberger_info_position-1].B;
	G=f4buchberger_info[f4buchberger_info_position-1].G;
	continue;
      }
      if (learning){
	f4buchberger_info.back().B=B;
	f4buchberger_info.back().G=G;
	f4buchberger_info.back().nonzero=added;
      }
      //unsigned debut=G.size()-added;
      // CERR << "finish loop G.size "<<G.size() << '\n';
      // CERR << added << '\n';
    } // end main loop
    if (debug_infolevel)
      CERR << CLOCK()*1e-6 << " # F4 steps " << age << '\n';
#ifdef GIAC_GBASIS_REDUCTOR_MAXSIZE
    // remove small size reductors that were kept despite not being in gbasis
    vector<unsigned> G1;
    for (unsigned i=0;i<G.size();++i){
      if (res[G[i]].in_gbasis)
	G1.push_back(G[i]);
    }
    G.swap(G1);
#endif
    if (coeffsmodptr){
      // FIXME gbasis is not interreduced. This might be too costly on Q
      if (!gparam.rawcoeffs)
        reduce_syzygy(*coeffsmodptr,resmodorig,env);
    }
    // convert back zpolymod<tdeg_t,modint_t> to polymod<tdeg_t,modint_t_t>
    // if eliminate_flag is true, keep only basis element that do not depend
    // on variables to eliminate
    if (eliminate_flag && (order.o==_3VAR_ORDER || order.o>=_7VAR_ORDER)){
      resmod.clear();
      resmod.reserve(res.size());
      for (unsigned l=0;l<res.size();++l){
	tdeg_t d=res[l].ldeg;
#ifndef GBASIS_NO_OUTPUT
	if (d.vars64()){
	  if (d.tab[0]%2){
	    if (d.tab[0]/2)
	      continue;
	  }
	  else {
	    if (d.tdeg)
	      continue;
	  }
	}
	else {
	  if (d.tab[0])
	    continue;
	}
#endif
	resmod.resize(resmod.size()+1);
	convert(res[l],resmod.back());
      }
      res.clear();
      G.resize(resmod.size());
      for (unsigned j=0; j<resmod.size();++j)
	G[j]=j;
      // final interreduce step2
      polymod<tdeg_t,modint_t> TMP1(order,dim);
      for (unsigned j=0; j<resmod.size();++j){
	reducesmallmod(resmod[j],resmod,G,j,env,TMP1,true);
      }
    }
    else{
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " zfinal interreduction begin " << G.size() << '\n';
      resmod.resize(res.size());
      for (unsigned l=0;l<res.size();++l){
	resmod[l].coord.clear();
      }
      int val=-1;
      if (!coeffsmodptr
          //generators<100
	  //parallel>1 && threads_allowed && G.size()>=200
	  ){ // FIXME interreduce with coeffsmodptr
	val=zinterreduce_convert<tdeg_t,modint_t,modint_t2>(res,G,env,learning,learned_position,pairs_reducing_to_zero,f4buchberger_info,f4buchberger_info_position,recomputeR,-1/* age*/,multimodular,parallel,resmod,interred);
	if (debug_infolevel && val<0)
	  CERR << "zinterreduce failure" << '\n';
	// zfinal_interreduce(resmod,G,env,parallel); // res->resmod must be done. discarded because too slow mem locks
      }
      if (val<0 || val==12345){ 
	for (unsigned l=0;l<G.size();++l){
	  convert(res[G[l]],resmod[G[l]]);
	}
	res.clear();
      }
      if (val<0 && interred
          //&& !coeffsmodptr // gbasis coeffs with interreduce? for cyclic6, we have Partial number of monoms 2628059 without and 4762976 with
          ){
	// final interreduce step2
	// by construction resmod[G[j]] is already interreduced by resmod[G[k]] for k<j
	polymod<tdeg_t,modint_t> TMP1(order,dim);
	for (int j=int(G.size())-1; j>=0;--j){
	  if (debug_infolevel>1){
	    if (j%10==0){ CERR << "+"; CERR.flush();}
	    if (j%500==0){ CERR << CLOCK()*1e-6 << " remaining " << j << '\n';}
	  }
	  if (!start_index_v.empty() && G[j]<start_index_v.back())
	    start_index_v.pop_back();
	  if (0 && order.o==_REVLEX_ORDER) // this optimization did not work for cyclic10mod (at least when max_pairs=4096) 
	    reducesmallmod(resmod[G[j]],resmod,G,j,env,TMP1,true,start_index_v.empty()?0:start_index_v.back(),false,coeffsmodptr?&(*coeffsmodptr)[G[j]]:0,coeffsmodptr);
	  else // full interreduction since Buchberger steps do not interreduce
	    reducesmallmod(resmod[G[j]],resmod,G,j,env,TMP1,true,0,false,coeffsmodptr?&(*coeffsmodptr)[G[j]]:0,coeffsmodptr);
	}
      }
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " zfinal interreduction end " << G.size() << '\n';      
    }
    if (ressize<resmod.size())
      res.resize(ressize);
    if (debug_infolevel){ // was debug_infolevel>1
      unsigned t=0;
      for (unsigned i=0;i<resmod.size();++i)
	t += unsigned(resmod[i].coord.size());
      CERR << CLOCK()*1e-6 << " total number of monomials in gbasis " << t << " basis size " << G.size() << '\n';
      CERR << "Number of monomials cleared " << cleared << '\n';
      if (coeffsmodptr){
        for (unsigned i=0;i<G.size();++i){
          const vector<polymod<tdeg_t,modint_t> > & v=(*coeffsmodptr)[G[i]];
          for (unsigned j=0;j<v.size();++j)
            t += v[j].coord.size();
        }
        CERR << "Number of monomials in gbasis and coeffs " << t << '\n';
      }
    }
    smod(resmod,env);
    return true;
  }

  template<class tdeg_t,class modint_t>
  void remove_zero(vectpolymod<tdeg_t,modint_t> &gbmod){
    for (int i=0;i<gbmod.size();){
      if (gbmod[i].coord.empty())
	gbmod.erase(gbmod.begin()+i);
      else
	++i;
    }
  }

  template<class tdeg_t,class modint_t>
  int rur_quotient_ideal_dimension(const vectpolymod<tdeg_t,modint_t> & gbmod,polymod<tdeg_t,modint_t> & lm,polymod<tdeg_t,modint_t> * rurgblmptr=0,polymod<tdeg_t,modint_t> * rurlmptr=0);

void G_idn(vector<unsigned> & G,size_t s){
    G.resize(s);
    for (size_t i=0;i<s;++i)
      G[i]=i;
  }

  template<class tdeg_t>
  bool rur_compute(vectpolymod<tdeg_t,mod4int> & gbmod,polymod<tdeg_t,mod4int> & lm,polymod<tdeg_t,mod4int> & lmmodradical,mod4int p,polymod<tdeg_t,modint> & s,vector<int> * initsep,vectpolymod<tdeg_t,mod4int> & rur);
  template<class tdeg_t>
  bool rur_compute(vectpolymod<tdeg_t,modint> & gbmod,polymod<tdeg_t,modint> & lm,polymod<tdeg_t,modint> & lmmodradical,int p,polymod<tdeg_t,modint> & s,vector<int> * initsep,vectpolymod<tdeg_t,modint> & rur);


  template<class tdeg_t,class modint_t,class modint_t2>
  bool zgbasisrur(vectpoly8<tdeg_t> & res8,vectpolymod<tdeg_t,modint_t> &resmod,vector<unsigned> & G,modint_t env,bool totdeg,vector< paire > * pairs_reducing_to_zero,vector< zinfo_t<tdeg_t> > & f4buchberger_info,bool recomputeR,bool convertpoly8,bool eliminate_flag,bool multimodular,int parallel,bool interred,int & rurinzgbasis,vectpolymod<tdeg_t,modint_t> &rurv,polymod<tdeg_t,modint> & rurs,vector<int> * initsep,polymod<tdeg_t,modint_t> & rurlm,polymod<tdeg_t,modint_t> &rurlmmodradical,polymod<tdeg_t,modint_t> * rurgblmptr,polymod<tdeg_t,modint_t> * rurlmptr,const gbasis_param_t & gparam,vector< vectpolymod<tdeg_t,modint_t> > * coeffsmodptr){
    if (1 || 
	rurinzgbasis>=0){
      for (unsigned i=0;i<resmod.size();++i)
	resmod[i].coord.clear();
      // if rurinzgbasis<0 then it is -(number of elements) of the gbasis in Q
      // already computed with previous primes, we do not have to compute
      // the basis again, we just need to reduce the Q gbasis that is inside
      // res8[0..rurinzgbasis-1] 
      convert(res8,resmod,env,rurinzgbasis<0?-rurinzgbasis:0,!coeffsmodptr/* unitarize*/);
    }
    unsigned ressize = unsigned(res8.size());
    bool b=rurinzgbasis<0?true:in_zgbasis<tdeg_t,modint_t,modint_t2>(resmod,ressize,G,env,totdeg,pairs_reducing_to_zero,f4buchberger_info,recomputeR,eliminate_flag,multimodular,parallel,interred,gparam,coeffsmodptr);
    if (rurinzgbasis==1 || rurinzgbasis<0){
      vectpolymod<tdeg_t,modint_t> gbmod; 
      if (rurinzgbasis==1){
	gbmod.resize(G.size());
	for (int i=0;i<G.size();++i){
	  gbmod[i]=resmod[G[i]];
	}
	remove_zero(gbmod);
	sort(gbmod.begin(),gbmod.end(),tripolymod_tri<polymod<tdeg_t,modint_t> >(0));
      }
      else {
	G_idn(G,res8.size());
	gbmod=resmod;
	gbmod.resize(G.size());
      }
      int rqi=rur_quotient_ideal_dimension(gbmod,rurlm,rurgblmptr,rurlmptr);
      rurinzgbasis=rur_compute<tdeg_t>(gbmod,rurlm,rurlmmodradical,env,rurs,initsep,rurv);
    }
    else
      rurinzgbasis=0;
#ifndef GBASIS_4PRIMES
    if (convertpoly8)
      convert(resmod,res8,env);
#endif
    return b;
  }

  // Improvements planned for the future for computations over Q
  // Group primes by 4 using mod4int and mod4int2 types
  // for modint_t and modint_t2 instead of modint and modint2
  // The first part of linear algebra (sparse part) would be done
  // with the 4 primes in parallel, with little cost using SIMD instructions
  // This would also increase confidence that a monomial is not missing
  // during the computation (accidental cancellation for the 1st prime used)
  // and that all the learned stuff is correct
  // The second part of linear algebra (dense part) would be run individually
  // first (later we could make a 4 primes rref at the cost of a larger memory
  // footprint, but probably not too large since later runs have much less
  // non-0 rows than the initial run)
  //
  // Rur improvements: see https://arxiv.org/abs/2402.07141
  template<class tdeg_t,class modint_t,class modint_t2>
  bool zgbasis(vectpoly8<tdeg_t> & res8,vectpolymod<tdeg_t,modint_t> &resmod,vector<unsigned> & G,modint env,bool totdeg,vector< paire > * pairs_reducing_to_zero,vector< zinfo_t<tdeg_t> > & f4buchberger_info,bool recomputeR,bool convertpoly8,bool eliminate_flag,bool multimodular,int parallel,bool interred,vector<int> * initsep,const gbasis_param_t & gparam,vector< vectpolymod<tdeg_t,modint_t> > * coeffsmodptr){
    vectpolymod<tdeg_t,modint_t> rurv; 
    polymod<tdeg_t,modint> rurs;
    polymod<tdeg_t,modint_t> rurlm,rurlmmodradical;
    int rurinzgbasis=0;
    polymod<tdeg_t,modint_t> * Nullptr=0;
    return zgbasisrur<tdeg_t,modint_t,modint_t2>(res8,resmod,G,env,totdeg,pairs_reducing_to_zero,f4buchberger_info,recomputeR,convertpoly8,eliminate_flag,multimodular,parallel,interred,rurinzgbasis,rurv,rurs,initsep,rurlm,rurlmmodradical,Nullptr,Nullptr,gparam,coeffsmodptr);
  }
#endif // GIAC_SHORTSHIFTTYPE==16
  /* *************
     END ZPOLYMOD
     ************* */
  template<class tdeg_t>
  bool is_gbasis(const vectpoly8<tdeg_t> & res,double eps,bool modularcheck){
    if (res.empty())
      return false;
    if (debug_infolevel>0)
      CERR << "basis size " << res.size() << '\n';
    // build possible pairs (i,j) with i<j
    vector< vector<tdeg_t> > lcmpairs(res.size());
    vector<unsigned> G; G_idn(G,res.size());
    vectpoly8<tdeg_t> vtmp,tocheck;
    vector< paire > tocheckpairs;
    if (eps>0 && eps<2e-9)
      modularcheck=true;
    if (modularcheck)
      tocheck.reserve(res.size()*10); // wild guess
    else
      tocheckpairs.reserve(res.size()*10);
    order_t order=res.front().order;
    int dim=res.front().dim;
    poly8<tdeg_t> TMP1(order,res.front().dim),TMP2(TMP1),
      spol(TMP1),spolred(TMP1);
    polymod<tdeg_t,modint> spolmod(order,dim),TMP1mod(order,dim);
    vectpolymod<tdeg_t,modint> resmod;
    for (unsigned i=0;i<res.size();++i){
      const poly8<tdeg_t> & h = res[i];
      const tdeg_t & h0=h.coord.front().u;
      vector<tdeg_t> tmp(res.size());
      for (unsigned j=i+1;j<res.size();++j){
	index_lcm(h0,res[j].coord.front().u,tmp[j],h.order); 
      }
      swap(lcmpairs[i],tmp);
    }
    for (unsigned i=0;i<res.size();++i){    
      if (debug_infolevel>1)
	CERR << "checking pairs for i="<<i<<", j=";
      const poly8<tdeg_t> & resi = res[i];
      const tdeg_t & resi0=resi.coord.front().u;
      for (unsigned j=i+1;j<res.size();++j){
	if (disjoint(resi0,res[j].coord.front().u,order,dim))
	  continue;
	// criterion M, F
	unsigned J=0;
	tdeg_t & lcmij=lcmpairs[i][j];
	for (;J<i;++J){
	  if (tdeg_t_all_greater(lcmij,lcmpairs[J][j],order))
	    break;
	}
	if (J<i)
	  continue; 
	for (++J;J<j;++J){
	  tdeg_t & lcmJj=lcmpairs[J][j];
	  if (tdeg_t_all_greater(lcmij,lcmJj,order) && lcmij!=lcmJj)
	    break;
	}
	if (J<j)
	  continue; 
	// last criterion
	unsigned k;
	for (k=j+1;k<res.size();++k){
	  if (lcmpairs[i][k]!=lcmij && lcmpairs[j][k]!=lcmij
	      && tdeg_t_all_greater(lcmij,res[k].coord.front().u,order))
	    break;
	}
	if (k<res.size())
	  continue;
	// compute and reduce s-poly
	if (debug_infolevel>1)
	  CERR <<  j << ",";
	if (modularcheck){
	  spoly(resi,res[j],spol,TMP1,0);
	  tocheck.push_back(poly8<tdeg_t>(order,dim));
	  swap(tocheck.back(),spol);
	}
	else
	  tocheckpairs.push_back(paire(i,j));
      } // end j loop
      if (debug_infolevel>1)
	CERR << '\n';
    }
    if (debug_infolevel>0)
      CERR << "Number of critical pairs to check " << (modularcheck?tocheck.size():tocheckpairs.size()) << '\n';
    if (modularcheck) // modular check is sometimes slow
      return checkf4buchberger<tdeg_t,modint>(tocheck,res,G,-1,eps); // split version is slower!
    // integer check or modular check for one modulus (!= from first prime already used)
    modint p=(prevprime((1<<29)-30000000)).val;
    if (eps>0)
      convert(res,resmod,p);
    // FIXME should be parallelized
    for (unsigned i=0;i<tocheckpairs.size();++i){
#ifdef TIMEOUT
      control_c();
#endif
      if (interrupted || ctrl_c){
	CERR << "Check interrupted, assuming Groebner basis. Press Ctrl-C again to interrupt computation" << '\n';
	interrupted=ctrl_c=false;
	return true;
      }
      if (eps>0){
	spolymod<tdeg_t,modint>(resmod[tocheckpairs[i].first],resmod[tocheckpairs[i].second],spolmod,TMP1mod,p);
	reducemod(spolmod,resmod,G,-1,TMP1mod,p);
	// gen den; heap_reduce(spol,res,G,-1,vtmp,spolred,TMP1,den,0);
	if (!TMP1mod.coord.empty())
	  return false;
      }
      else {
	spoly(res[tocheckpairs[i].first],res[tocheckpairs[i].second],spol,TMP1,0);
	reduce(spol,res,G,-1,vtmp,spolred,TMP1,TMP2,0);
	// gen den; heap_reduce(spol,res,G,-1,vtmp,spolred,TMP1,den,0);
	if (!spolred.coord.empty())
	  return false;
      }
      if (debug_infolevel>0){
	CERR << "+";
	if (i%512==511)
	  CERR << tocheckpairs.size()-i << " remaining" << '\n'; 
      }
    }
    if (debug_infolevel)
      CERR << '\n' << "Successful check of " << tocheckpairs.size() << " critical pairs" << '\n';
    return true;
  }


  /* *************
     RUR UTILITIES (rational univariate representation for 0 dimension ideals)
     ************* */
  int rur_dim(int dim,order_t order){
    if (order.o==_3VAR_ORDER) return 3;
    if (order.o==_7VAR_ORDER) return 7;
    if (order.o==_11VAR_ORDER) return 11;
    if (order.o==_16VAR_ORDER) return 16;
    if (order.o==_32VAR_ORDER) return 32;
    if (order.o==_48VAR_ORDER) return 48;
    if (order.o==_64VAR_ORDER) return 64;
    return dim;
  }

  template<class tdeg_t,class modint_t> int compare_gblm(const polymod<tdeg_t,modint_t> & a,const polymod<tdeg_t,modint_t> & b){
    int as=a.coord.size(),bs=b.coord.size();
    order_t order=a.order;
    for (int i=0;i<as && i<bs;++i){
      tdeg_t ua=a.coord[i].u,ub=b.coord[i].u;
      if (ua==ub) continue;
      return tdeg_t_greater(ua,ub,order)?1:-1;
    }
    if (as==bs) return 0;
    return as>bs?1:-1;
  }
  // list of leadings coefficients of the gbasis
  template<class tdeg_t,class modint_t> void rur_gblm(const vectpolymod<tdeg_t,modint_t> & gbmod,polymod<tdeg_t,modint_t> & gblm){
    gblm.coord.clear();
    unsigned S = unsigned(gbmod.size());
    if (S){
      gblm.order=gbmod[0].order;
      gblm.dim=gbmod[0].dim;
    }
    for (unsigned i=0;i<S;++i){
      if (gbmod[i].coord.empty())
	continue;
      gblm.coord.push_back(gbmod[i].coord.front());
    }
  }

  template<class tdeg_t,class modint_t> void rur_gblm1(const vectpolymod<tdeg_t,modint_t> & gbmod,polymod<tdeg_t,modint_t> & gblm){
    unsigned S = unsigned(gbmod.size());
    for (unsigned i=0;i<S;++i){
      if (gbmod[i].coord.empty())
	continue;
      gblm.coord.push_back(gbmod[i].coord.front());
      gblm.coord.back().g=1;
    }
  }

  // returns -1 if not 0 dimensional, -RAND_MAX if overflow
  // otherwise returns dimension of quotient and sets lm to the list of 
  // leading monomials generating the quotient ideal
  template<class tdeg_t,class modint_t>
  int rur_quotient_ideal_dimension(const vectpolymod<tdeg_t,modint_t> & gbmod,polymod<tdeg_t,modint_t> & lm,polymod<tdeg_t,modint_t> * rurgblmptr,polymod<tdeg_t,modint_t> * rurlmptr){
    if (gbmod.empty())
      return -1;
    order_t order=gbmod.front().order;
    int dim=gbmod.front().dim;
    unsigned S = unsigned(gbmod.size());
    lm.order=order; lm.dim=dim; lm.coord.clear();
    polymod<tdeg_t,modint_t> gblm(order,dim);
    rur_gblm(gbmod,gblm);
    if (rurgblmptr && rurlmptr){
      bool chk;
#ifdef HAVE_LIBPTHREAD
      int locked=pthread_mutex_trylock(&rur_mutex);
      if (locked)
	chk = false;
      else 
	chk = gblm==*rurgblmptr;
#else
      chk = gblm==*rurgblmptr;
#endif
      if (chk)
	lm=*rurlmptr;
#ifdef HAVE_LIBPTHREAD
      if (locked) pthread_mutex_unlock(&rur_mutex);	
#endif
      if (chk)
	return lm.coord.size();
    }
    //#define RUR_IDEAL_JSTOP
#ifdef RUR_IDEAL_JSTOP
    vector<int> jstart;
    if (order.o==_REVLEX_ORDER){
      // record positions where total degree appears first in gblm
      jstart.resize(gblm.coord.back().u.total_degree(order)+1);
      int prevtdeg=-1;
      for (int j=0;j<S;++j){
	int curtdeg=gblm.coord[j].u.total_degree(order);
	if (curtdeg>prevtdeg){
	  ++prevtdeg;
	  for (;;){
	    jstart[prevtdeg]=j;
	    if (prevtdeg==curtdeg)
	      break;
	    ++prevtdeg;
	  }
	}
      }
    }
#endif
    // for 3var, 7var, 11 var search in the first 3 var, 7 var or 11 var
    // for revlex search for all variables
    // we must find a leading monomial in gbmod that contains only this variable
    int d=rur_dim(dim,order);
    vector<int> v(d);
    for (unsigned i=0;i<S;++i){
      index_t l;
      get_index(gblm.coord[i].u,l,order,dim);
      unsigned j,k;
      for (j=0;int(j)<d;++j){
	if (l[j]){
	  for (k=j+1;int(k)<d;++k){
	    if (l[k])
	      break;
	  }
	  if (k==d)
	    v[j]=l[j];
	  break;
	}
      }
    }
    // now all indices of v must be non 0
    double M=1;
    for (unsigned i=0;i<v.size();++i){
      if (v[i]==0)
	return -1;
      M *= v[i];
    }
    if (debug_infolevel)
      CERR << CLOCK() << " rur quotient ideal " << M << "\n";
    if (M>1e10)
      return -RAND_MAX; // overflow
    // the ideal is finite dimension, now we will compute the exact dimension
    // a monomial degree is associated to an integer with
    // [l_0,l_1,...,l_{d-1}] -> ((l_0*v1+l_1)*v2+...+l_{d-1} < v0*v1*...
    // perhaps a sieve would be faster, but it's harder to implement
    // and we won't consider too high order anyway...
    index_t cur(d);
    for (longlong I=0;I<M;++I){
      longlong i=I;
      // i-> cur -> tdeg_t
      for (int j=int(v.size())-1;j>=0;--j){
	longlong q=i/v[j];
	cur[j]=i-q*v[j];
	i=q;
      }
      tdeg_t curu(cur,order);
      // then search if > to one of the leading monomials for all indices
      unsigned j=-1;
      if (order.o==_3VAR_ORDER){
	for (j=0;j<S;++j){
	  tdeg_t u=gblm.coord[j].u;
	  if (curu.tab[1]>=u.tab[1] && curu.tab[2]>=u.tab[2] && curu.tab[3]>=u.tab[3])
	    break;
	}
      }
      if (order.o==_7VAR_ORDER){
      }
      if (order.o==_11VAR_ORDER){
      }
      if (order.o==_REVLEX_ORDER){
	int curtdeg=curu.total_degree(order),jstop;
	j=0; jstop=S;
#ifdef RUR_IDEAL_JSTOP
	//if (curtdeg>=jstart.size()) j=S;
	if (curtdeg+1>=jstart.size())
	  jstop=S;
	else
	  jstop=jstart[curtdeg+1];
	//CERR << "jstop " << jstop << '\n';
#endif
	for (;j<jstop;++j){
	  if (tdeg_t_all_greater(curu,gblm.coord[j].u,order))
	    break;
	}
	if (j==jstop) j=S;
      }
      if (j==gbmod.size()) // not found, add cur to the list of monomials
	lm.coord.push_back(T_unsigned<modint_t,tdeg_t>(create<modint_t>(1),curu));
      else {
	int D=d;
	while (D>=2 && cur[D-1]==0) --D;
	if (D!=d || cur[D-1]!=v[D-1]-1){
	  // increase I to the next multiple
	  longlong prod=v[d-1];
	  for (;D<d;++D)
	    prod *= v[D-1];
	  I /= prod;
	  ++I;
	  I *= prod;
	  --I;
	}
      }
    }
    sort(lm.coord.begin(),lm.coord.end(),tdeg_t_sort_t<tdeg_t>(order));
    if (rurgblmptr && rurlmptr){
#ifdef HAVE_LIBPTHREAD
      int locked=pthread_mutex_trylock(&rur_mutex);
      if (locked)
	return unsigned(lm.coord.size());
#endif
      *rurgblmptr=gblm;
      *rurlmptr=lm;
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&rur_mutex);
#endif
    }
    return unsigned(lm.coord.size());
  }

  template<class tdeg_t,class modint_t>
  void rur_mult(const polymod<tdeg_t,modint_t> & a,const polymod<tdeg_t,modint_t> & b,modint p,polymod<tdeg_t,modint_t> & res,polymod<tdeg_t,modint_t> &tmp){
    res.coord.clear();
    for (unsigned i=0;i<b.coord.size();++i){
      smallmultsubmodshift(res,0,(-b.coord[i].g) % p,a,b.coord[i].u,tmp,p);
      tmp.coord.swap(res.coord);
    }
  }
  
  // multiply a by b mod p in res
  // b is supposed to have small length
  template<class tdeg_t,class modint_t>
  void rur_mult(const polymod<tdeg_t,modint_t> & a,const polymod<tdeg_t,modint_t> & b,modint p,polymod<tdeg_t,modint_t> & res){
    polymod<tdeg_t,modint_t> tmp(b.order,b.dim);
    rur_mult(a,b,p,res,tmp);
  }

  // coordinates of cur w.r.t. lm
  template<class tdeg_t,class modint_t>
  void rur_coordinates(const polymod<tdeg_t,modint_t> & cur,const polymod<tdeg_t,modint_t> & lm,vecteur & tmp,vector<bool> * ptr=0){
    unsigned k=0,j=0;
    for (;j<lm.coord.size() && k<cur.coord.size();++j){
      if (lm.coord[j].u!=cur.coord[k].u)
	tmp[j]=0;
      else {
	tmp[j]=cur.coord[k].g;
	if (ptr) (*ptr)[j]=true;
	++k;
      }
    }
    for (;j<lm.coord.size();++j){
      tmp[j]=0;
    }
  }

  template<class tdeg_t,class modint_t>
  void rur_coordinates(const polymod<tdeg_t,modint_t> & cur,const polymod<tdeg_t,modint_t> & lm,vector<int> & tmp,vector<bool> * ptr=0){
    unsigned k=0,j=0;
    for (;j<lm.coord.size() && k<cur.coord.size();++j){
      if (lm.coord[j].u!=cur.coord[k].u)
	tmp[j]=0;
      else {
	tmp[j]=cur.coord[k].g;
	if (ptr) (*ptr)[j]=true;
	++k;
      }
    }
    for (;j<lm.coord.size();++j){
      tmp[j]=0;
    }
  }

  void rur_cleanmod(vecteur & m){
    for (unsigned i=0;i<m.size();++i){
      if (m[i].type==_MOD)
	m[i]=*m[i]._MODptr;
    }
  }

  // s*each dim coordinates reduced as a linear combination of the lines of M
  // (called with s==1)
  template<class tdeg_t,class modint_t>
  bool rur_linsolve(const vectpolymod<tdeg_t,modint_t> & gbmod,const polymod<tdeg_t,modint_t> & lm,const polymod<tdeg_t,modint_t> & s,const matrice & M,modint_t p,matrice & res){
    int S=int(lm.coord.size()),dim=lm.dim;
    if (M.size()==1+dim){
      // M is not the matrix of the system, it is already a kernel
      for (int i=1;i<=dim;++i){
	gen g=M[i];
	if (g.type!=_VECT) return false;
	vecteur m(*g._VECTptr);
	if (m.size()>S){
	  rur_cleanmod(m);
	  if (m[m.size()-(dim-i)-1]!=-1) return false;
	  m[m.size()-(dim-i)-1]=0;
	}
	reverse(m.begin(),m.end());
	m=trim(m,0);
	res.push_back(m);
      }
      return true;
    }
    order_t order=lm.order;
    polymod<tdeg_t,modint_t> TMP1(order,dim);
    vector<unsigned> G; G_idn(G,gbmod.size());
    matrice N(M);
    polymod<tdeg_t,modint_t> si(order,dim);
    int d=rur_dim(dim,order);
    vecteur tmp(lm.coord.size());
    for (unsigned i=0;int(i)<d;++i){
      index_t l(dim);
      l[i]=1;
      smallshift(s.coord,tdeg_t(l,order),si.coord);
      reducesmallmod(si,gbmod,G,-1,p,TMP1,false);
      // get coordinates of cur in tmp (make them mod p)
      rur_coordinates(si,lm,tmp);
      N.push_back(tmp);
    }
    N=mtran(N);
    if (debug_infolevel)
      CERR << CLOCK()*1e-6 << " rur rref" << '\n';
    gen n=_rref(N,context0);
    if (!ckmatrix(n))
      return false;
    N=mtran(*n._VECTptr);
    // check that first line are idn
    for (unsigned i=0;i<lm.coord.size();++i){
      vecteur & Ni=*N[i]._VECTptr;
      for (unsigned j=0;j<lm.coord.size();++j){
	if (i==j && !is_one(Ni[j]))
	  return false;
	if (i!=j && !is_zero(Ni[j]))
	  return false;
      }
    }
    N=vecteur(N.begin()+lm.coord.size(),N.end());
    for (unsigned i=0;i<N.size();++i){
      vecteur & m =*N[i]._VECTptr;
      for (unsigned j=0;j<m.size();++j){
	if (m[j].type==_MOD)
	  m[j]=*m[j]._MODptr;
      }
      reverse(m.begin(),m.end());
      m=trim(m,0);
    }
    res=N;
    return true;
  }
  // scalar product assuming all coordinates are positive
  int multmod_positive(const vector<int> & v, const vector<int> & w,int p,longlong res=0){
    longlong p2=extend(p)*p,p4=4*p2;
    vector<int>::const_iterator it=v.begin(),itend=v.end(),it4=itend-4,jt=w.begin(),jtend=w.end();
    if (p2<(1ULL<<59)){
      for (;it<it4;jt+=4,it+=4){
	res += extend(*it)*(*jt)+extend(it[1])*jt[1]+extend(it[2])*jt[2]+extend(it[3])*jt[3];
	res -= p4;
	res += (res>>63)&p4;
      }
    }
    for (; it!=itend;++jt,++it){
      //if (!*it) continue;
      res += extend(*it)*(*jt);
      res -= p2;
      res += (res>>63)&p2;
    }
    res %= p;
    //if (res<0) CERR << "bug\n";
    return res;
  }


  void multmod_positive4(const vector<int> & v1, const vector<int> & v2,const vector<int> & v3,const vector<int> & v4,const vector<int> & w,int p,int &res1,int & res2,int & res3,int & res4){
    longlong r1=res1,r2=res2,r3=res3,r4=res4;
    longlong p2=extend(p)*p,p4=4*p2;
    vector<int>::const_iterator it1=v1.begin(),itend=v1.end(),itend4=itend-4,it2=v2.begin(),it3=v3.begin(),it4=v4.begin(),jt=w.begin(),jtend=w.end();
#if defined CPU_SIMD && defined __AVX2__
    Vec4q R1(0),R2(0),R3(0),R4(0),p44(4*p2),V1,V2,V3,V4,w4,w4s;
    itend4=itend-15; // itend8
    if (p2<(1ULL<<59)){
      for (;it1<itend4;jt+=8,it4+=8,it3+=8,it2+=8,it1+=8){
	w4.load(&*jt); // load 8 values
	w4s = w4>>32;
	V1.load(&*it1);
	R1 += _mm256_mul_epi32(w4,V1); // 4 products
	V1 >>= 32;
	R1 += _mm256_mul_epi32(w4s,V1); // 4 products
	V2.load(&*it2);
	R2 += _mm256_mul_epi32(w4,V2); // 4 products
	V2 >>= 32;
	R2 += _mm256_mul_epi32(w4s,V2); // 4 products
	V3.load(&*it3);
	R3 += _mm256_mul_epi32(w4,V3); // 4 products
	V3 >>= 32;
	R3 += _mm256_mul_epi32(w4s,V3); // 4 products
	V4.load(&*it4);
	R4 += _mm256_mul_epi32(w4,V4); // 4 products
	V4 >>= 32;
	R4 += _mm256_mul_epi32(w4s,V4); // 4 products
	jt+=8;it4+=8;it3+=8;it2+=8;it1+=8;
	w4.load(&*jt); // load 8 values
	w4s = w4>>32;
	V1.load(&*it1);
	R1 += _mm256_mul_epi32(w4,V1); // 4 products
	V1 >>= 32;
	R1 += _mm256_mul_epi32(w4s,V1); // 4 products
	R1 -= p44;
	R1 += (R1>>63)&p44;
	V2.load(&*it2);
	R2 += _mm256_mul_epi32(w4,V2); // 4 products
	V2 >>= 32;
	R2 += _mm256_mul_epi32(w4s,V2); // 4 products
	R2 -= p44;
	R2 += (R2>>63)&p44;
	V3.load(&*it3);
	R3 += _mm256_mul_epi32(w4,V3); // 4 products
	V3 >>= 32;
	R3 += _mm256_mul_epi32(w4s,V3); // 4 products
	R3 -= p44;
	R3 += (R3>>63)&p44;
	V4.load(&*it4);
	R4 += _mm256_mul_epi32(w4,V4); // 4 products
	V4 >>= 32;
	R4 += _mm256_mul_epi32(w4s,V4); // 4 products
	R4 -= p44;
	R4 += (R4>>63)&p44;
      }
    }
#else
    if (p2<(1ULL<<59)){
      for (;it1<itend4;jt+=4,it4+=4,it3+=4,it2+=4,it1+=4){
	longlong j0=*jt,j1=jt[1],j2=jt[2],j3=jt[3];
	r1 += (*it1)*j0+it1[1]*j1+it1[2]*j2+it1[3]*j3;
	r1 -= p4;
	r1 += (r1>>63)&p4;
	r2 += (*it2)*j0+it2[1]*j1+it2[2]*j2+it2[3]*j3;
	r2 -= p4;
	r2 += (r2>>63)&p4;
	r3 += (*it3)*j0+it3[1]*j1+it3[2]*j2+it3[3]*j3;
	r3 -= p4;
	r3 += (r3>>63)&p4;
	r4 += (*it4)*j0+it4[1]*j1+it4[2]*j2+it4[3]*j3;
	r4 -= p4;
	r4 += (r4>>63)&p4;
      }
    }
#endif      
#if defined CPU_SIMD && defined __AVX2__
    p4 = 2*p2;
    r1 = res1+R1.extract(0)+R1.extract(1);
    r1 -= p4;
    r1 += (r1>>63)&p4;
    r1 += R1.extract(2)+R1.extract(3);
    r1 -= p4;
    r1 += (r1>>63)&p4;
    r2 = res2+R2.extract(0)+R2.extract(1);
    r2 -= p4;
    r2 += (r2>>63)&p4;
    r2 += R2.extract(2)+R2.extract(3);
    r2 -= p4;
    r2 += (r2>>63)&p4;
    r3 = res3+R3.extract(0)+R3.extract(1);
    r3 -= p4;
    r3 += (r3>>63)&p4;
    r3 += R3.extract(2)+R3.extract(3);
    r3 -= p4;
    r3 += (r3>>63)&p4;
    r4 = res4+R4.extract(0)+R4.extract(1);
    r4 -= p4;
    r4 += (r4>>63)&p4;
    r4 += R4.extract(2)+R4.extract(3);
    r4 -= p4;
    r4 += (r4>>63)&p4;
#endif
    for (; it1!=itend;++jt,++it4,++it3,++it2,++it1){
      longlong j=*jt;
      r1 += *it1*j;
      r1 -= p2;
      r1 += (r1>>63)&p2;
      r2 += *it2*j;
      r2 -= p2;
      r2 += (r2>>63)&p2;
      r3 += *it3*j;
      r3 -= p2;
      r3 += (r3>>63)&p2;
      r4 += *it4*j;
      r4 -= p2;
      r4 += (r4>>63)&p2;
    }
    res1 = r1%p;
    res2 = r2%p;
    res3 = r3%p;
    res4 = r4%p;
  }

// matrix vector multiplication assuming all coordinates are positive
  void multmod_positive(const vector< vector<int> > &m,const vector<int> & v,int p,vector<int> & mv){
    mv.resize(m.size());
    for (int i=0;i<m.size();++i)
      mv[i]=multmod_positive(m[i],v,p);
  }

  // partially sparse multiplication m*v: m is the dense part of the matrix
  // ms is the sparse part, as a vector of -1 or indices
  void multmod_positive(const vector< vector<int> > &m,const vector<int> &ms,const vector<int> & v,int p,vector<int> & mv){
    mv.resize(m.size());
    for (int i=0;i<mv.size();++i)
      mv[i]=0;
    vector<int> w;
    // set mv and w using ms
    for (int i=0;i<ms.size();++i){
      if (ms[i]<0)
	w.push_back(v[i]);
      else {
	mv[ms[i]]=v[i];
      }
    }
    // dense part of the multiplication
    int s=m.size()-4,i=0;
    for (;i<s;i+=4)
      multmod_positive4(m[i],m[i+1],m[i+2],m[i+3],w,p,mv[i],mv[i+1],mv[i+2],mv[i+3]);
    for (;i<m.size();++i)
      mv[i]=multmod_positive(m[i],w,p,mv[i]);
  }

  // partially sparse multiplication v*m: 
  // m is the transpose of the dense part of the matrix
  // ms is the sparse part, as a vector of -1 or indices
  void multmod_positive(const vector<int> & v,const vector< vector<int> > &m,const vector<int> &ms,int p,vector<int> & mv){
    mv.resize(v.size());
    int j=0;
    int i4[8],i4pos=0; // vector<int> i4;
    for (int i=0;i<v.size();++i){
      if (ms[i]>=0)
	mv[i]=v[ms[i]];
      else {
	i4[i4pos]=i; ++i4pos; // i4.push_back(i);
	i4[i4pos]=j; ++i4pos; // i4.push_back(j);
	mv[i]=0;
	if (i4pos==8){
	  multmod_positive4(m[i4[1]],m[i4[3]],m[i4[5]],m[i4[7]],v,p,mv[i4[0]],mv[i4[2]],mv[i4[4]],mv[i4[6]]);
	  i4pos=0;
	}
	++j;
      }
    }
    for (int i=0;i<i4pos;i+=2)
      mv[i4[i]]=multmod_positive(m[i4[i+1]],v,p);
  }

  // Compute minimal polynomial of s
  // If it has max degree, then M will contain
  //   M[0] = ? (minpoly if Hankel successful)
  //   M[j], j=1..dim the j-th coordinate
  // Otherwise M might be empty or
  //   i-th row of M : coordinates of s^i reduced/gbmod in terms of lm
  template<class tdeg_t,class modint_t,class modint_t2>
  bool rur_minpoly(const vectpolymod<tdeg_t,modint_t> & gbmod,const polymod<tdeg_t,modint_t> & lm,const polymod<tdeg_t,modint_t> & s,modint_t p,vecteur & m,matrice & M){
    int S=int(lm.coord.size()),dim=lm.dim;
    order_t order=lm.order;
    polymod<tdeg_t,modint_t> TMP1(order,dim),TMP2(order,dim);
    vector<unsigned> G; G_idn(G,gbmod.size());
    bool done=false;
    matrice chk;
    if (1){ 
      M.clear();
      if (debug_infolevel)
	CERR << CLOCK()*1e-6 << " rur separate " << s << " * monomial matrix computation " << S << '\n';
      // matrix of multiplication by s of all monomials in lm
      // stored as a partially sparse matrix in mults/multv
      polymod<tdeg_t,modint_t> cur(order,dim);
      vector<int> tmp(S),tmp1;
      vecteur tmpv(S);
      vector< vector<int> > mults(S,vector<int>(S)),tmpm; int multspos=0;
      vector<int> multv(S);
      polymod<tdeg_t,modint_t> gblm(order,dim);
      vector< polymod<tdeg_t,modint_t> > missed;
      rur_gblm(gbmod,gblm);
      reverse(gblm.coord.begin(),gblm.coord.end());
      int miss=0; bool missed_at_end=true; vector<int> missed_pos;
      for (int i=0;i<S;++i){
	cur.coord.clear();
	cur.coord.push_back(lm.coord[i]);
	rur_mult(s,cur,p,TMP1,TMP2);
	cur.coord.swap(TMP1.coord);
	bool red=false;
	if (cur.coord.size()==1 && cur.coord.front().g==1){
	  // if lm*s has one monomial
	  // it might be in the list of the basis monomials
	  // or it might be a leading coeff of one of the gbasis elements
	  typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=lm.coord.begin(),jtend=lm.coord.end(),jt_=jt;
	  if (dicho<tdeg_t,modint_t>(jt_,jtend,cur.coord.front().u,order)){
	    multv[i]=jt_-jt;
	    continue;
	  }
	  jt=gblm.coord.begin();jtend=gblm.coord.end();
	  if (dicho<tdeg_t,modint_t>(jt,jtend,cur.coord.front().u,order)){
	    int curpos=jtend-jt;
	    const polymod<tdeg_t,modint_t> & curgb=gbmod[curpos-1];
	    jt=curgb.coord.begin()+1; jtend=curgb.coord.end();
	    cur.coord.clear(); cur.coord.reserve(jtend-jt);
	    for (;jt!=jtend;++jt){
	      cur.coord.push_back(T_unsigned<modint_t,tdeg_t>(-jt->g,jt->u));
	    }
	    red=true;
	  }
	}
	if (!red){
	  miss++;
	  if (missed_at_end){
	    missed.push_back(cur); 
	    missed_pos.push_back(multspos); 
	    cur.coord.clear();
	  }
	  else
	    reducesmallmod(cur,gbmod,G,-1,p,TMP1,false);
	}
	multv[i]=-1;
	rur_coordinates(cur,lm,tmp);
	make_positive(tmp,p);
	mults[multspos].swap(tmp); ++multspos; // mults.push_back(tmp);
      }
      if (missed_at_end){
	bool doit=true;
	if (//0 && 
	    miss>=0.1*S){
	  doit=zsimult_reduce<tdeg_t,modint_t,modint_t2>(missed,gbmod,p,false,1); // done if it returns 0
	}
	if (doit) {
	  for (int i=0;i<miss;++i){
	    reducesmallmod(missed[i],gbmod,G,-1,p,TMP1,false);
	  }
	}
#if 1
	for (int i=0;i<miss;++i){
	  mults[missed_pos[i]].resize(tmp.size());
	}
	for (int i=0;i<miss;++i){
	  rur_coordinates(missed[i],lm,tmp);
	  make_positive(tmp,p);
	  mults[missed_pos[i]].swap(tmp);
	}
#else
	for (int i=0;i<miss;++i){
	  rur_coordinates(missed[i],lm,tmp);
	  make_positive(tmp,p);
	  mults[missed_pos[i]]=tmp;
	}
#endif
      }
      mults.resize(multspos);
      tran_vect_vector_int(mults,tmpm); tmpm.swap(mults);  
      if (debug_infolevel)
	CERR << CLOCK()*1e-6 << " missed " << miss << ", rur * xi" << '\n';
      // s^i is obtained by multiplying mults by the coordinates of s^[i-1]
      // tmpm rows are the coordinates of s*lm[i] if multv[i]==-1
      // mults is the transposed sparse
#if 1
      // For each coordinate, reduce x[i], i<dimension
      // coordinates of x[i] reduced in terms of lm is stored in Kxi
      // Code below is probably wrong for non_zero count
      int d=rur_dim(dim,order);
      vector< vector<int> > Kxi(d,vector<int>(S)); Kxi.reserve(d);
      polymod<tdeg_t,modint_t> si(order,dim);
      polymod<tdeg_t,modint_t> one(order,dim);
      one.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,tdeg_t(index_m(dim),order)));
      vector<bool> nonzero(S,false); vector<int> posxi(d,-1);
      index_t l(dim);
      for (unsigned i=0;int(i)<d;++i){
	for (int j=0;j<dim;++j)
	  l[j]=i==j?1:0;
	smallshift(one.coord,tdeg_t(l,order),si.coord);
	typename std::vector< T_unsigned<modint_t,tdeg_t> >::const_iterator jt=lm.coord.begin(),jtend=lm.coord.end(),jt_=jt;
	if (dicho<tdeg_t,modint_t>(jt_,jtend,si.coord.front().u,order)){
	  tmp.clear(); tmp.resize(S); tmp[jt_-jt]=1;
	  nonzero[jt_-jt]=true;
	  posxi[i]=jt_-jt;
	}
	else {
	  jt=gblm.coord.begin();jtend=gblm.coord.end();
	  if (dicho<tdeg_t,modint_t>(jt,jtend,si.coord.front().u,order)){
	    int curpos=jtend-jt;
	    const polymod<tdeg_t,modint_t> & curgb=gbmod[curpos-1];
	    jt=curgb.coord.begin()+1; jtend=curgb.coord.end();
	    si.coord.clear(); si.coord.reserve(jtend-jt);
	    for (;jt!=jtend;++jt){
	      si.coord.push_back(T_unsigned<modint_t,tdeg_t>(-jt->g,jt->u));
	    }
	  }
	  else 
	    reducesmallmod(si,gbmod,G,-1,p,TMP1,false);
	    // get coordinates of cur in tmp (make them mod p)
	  rur_coordinates(si,lm,tmp,&nonzero);
	  make_positive(tmp,p);
	}
	Kxi[i].swap(tmp);
      }
      int count=0; 
      for (int i=0;i<S;++i){ 
	if (nonzero[i]) count++; 
      }
      if (//0 && 
	  count<S/10){
	if (debug_infolevel>1) CERR << CLOCK()*1e-6 << "Hankel start\n" ;
	// IMPROVE: compute s^0 to s^[2S-1] (instead of s^0 to s^S)
	// take coordinate of index corresponding to 1 in lm
	// and find minpoly q using reverse_rsolve
	// Simultaneously, for each coordinate x1..x_d, 
	// find coordinates of x_i reduced in the list of monomials lm,
	// make scalar product with s^k
	// then solve Hankel system of SxS matrix with antidiagonals
	// the 1st coordinates above, and second member a vector with
	// components the scalar products with s^k for 0<=k<S
	// Hankel[g0,...,g_(2S-2)] is invertible in O(S^2) by
	// computing sum_i>=1 g_i z^(-i)=p/q at z=infinity, deg(q)=S, deg(p)<S
	// then solve u*p+v*q=1 and compute Bezoutian[q,u] 
	// https://en.wikipedia.org/wiki/B%C3%A9zout_matrix
	tmp.resize(S);//=vector<int>(S);
	for (int i=0;i<S;++i) 
	  tmp[i]=std_rand()/2;
	vector<int> g(2*S);
	vector< vector<int> > hankelsystb(d,vector<int>(S)); // second members of Hankel systems
	for (int i=0;i<S;++i){
	  for (int j=0;j<d;++j){
	    if (posxi[j]>=0)
	      hankelsystb[j][i]=tmp[posxi[j]];
	    else
	      hankelsystb[j][i]=multmod_positive(Kxi[j],tmp,p);
	  }
	  g[i]=tmp.back();
	  multmod_positive(tmp,tmpm,multv,p,tmp1); 
	  tmp.swap(tmp1);
	}
	if (debug_infolevel>1) CERR << CLOCK()*1e-6 << " Hankel mult part 2\n" ;
	for (int i=S;i<2*S;++i){
	  g[i]=tmp.back();
	  multmod_positive(tmp,tmpm,multv,p,tmp1); 
	  tmp.swap(tmp1);
	}
	if (debug_infolevel>1) CERR << CLOCK()*1e-6 << " Hankel mult end\n" ;
	vecteur V; vector_int2vecteur(g,V);
	reverse(V.begin(),V.end()); // degree(V)=2S-1, size(V)=2S
        V=trim(V,0);
	vecteur x2n(2*S+1),A,B,G,U,unused,D,tmp1,tmp2; x2n[0]=1; // x2n=x^(2*S)
	environment env; env.modulo=p; env.moduloon=true;
	if (
	    hgcd(x2n,V,p,G,U,D,B,unused,A,tmp1,tmp2)
	    ){
	  if (A.empty()){
	    // A=D*x2n+B*V
	    operator_times(B,V,&env,A);
	    // keep S terms (lower part)
	    if (A.size()>S)
	      A.erase(A.begin(),A.end()-S);
	  }
	  //egcd_pade(x2n,V,S,A,B,&env);
	  if (debug_infolevel) CERR << CLOCK()*1e-6 << " Hankel after Pade degrees " << S << "," << A.size() << "," << B.size() << "\n" ;
	  reverse(B.begin(),B.end());
	  while (B.size()<S+1)
	    B.push_back(0);
	  reverse(A.begin(),A.end());
	  while (A.size()<S)
	    A.push_back(0);
	  A=trim(A,0);
	  if (B.size()==S+1){
	    // B should be the min poly, normalize
	    if (B.front()!=0){
	      gen coeff=invmod(B.front(),p);
	      mulmodpoly(B,coeff,&env,B);
	      m=trim(B,0);
	      //modpoly mgcd=gcd(m,derivative(m,&env),&env);
	      mulmodpoly(A,coeff,&env,A);
	      // now Bezout 
	      egcd(A,m,&env,U,unused,D);
	      // check Bezout and also that B is squarefree
	      if (D.size()==1){ // D[0] should be 1
		// Bezoutian of U and B will invert Hankel matrix
		// compute Bezoutian(U,B)*Kxi
		vector<int> u,b;
		vecteur2vector_int(U,p,u);
		make_positive(u,p);
		vecteur2vector_int(B,p,b);
		make_positive(b,p);
		reverse(u.begin(),u.end()); reverse(b.begin(),b.end());
		while (u.size()<b.size())
		  u.push_back(0);
		while (b.size()<u.size())
		  b.push_back(0);
		// u and b have now size S+1
		longlong p2=extend(p)*p;
		// https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.85.3710&rep=rep1&type=pdf
		vector< vector<int> > bez(S,vector<int>(S));
		// initialization
		for (int i=0;i<S;++i){
		  for (int j=i;j<S;++j){
		    longlong r = extend(u[i])*b[j+1]-extend(b[i])*u[j+1];
		    r += (r>>63) & p2; // make r positive
		    bez[i][j]=r%p;
		  }
		}
		// recursion
		for (int i=1;i<=S-2;++i){
		  for (int j=i;j<=S-2;++j){
		    int r = bez[i][j];
		    r += bez[i-1][j+1]-p;
		    r += (r>>31) & p; // make r positive
		    bez[i][j] = r;
		  }
		}
		// symmetry
		for (int i=1;i<S;++i){
		  for (int j=0;j<i;++j)
		    bez[i][j]=bez[j][i];
		}
		// now compute bez*hankelsystb
		if (debug_infolevel>1) CERR << CLOCK()*1e-6 << " Hankel *\n" ;
		vector< vector<int> > Ker(d+1);
		vecteur2vector_int(m,p,Ker[0]);
		for (int i=0;i<d;++i)
		  multmod_positive(bez,hankelsystb[i],p,Ker[i+1]);
		vectvector_int2vecteur(Ker,M);
		if (debug_infolevel>1) CERR << CLOCK()*1e-6 << "Hankel end\n" ;
		return true;
	      } // end D.size()==1
	      else {
		if (1 && D.back()==0 && D[D.size()-2]==0){
                  DivRem(m,D,&env,unused,U);m=unused;//m.clear();
		  return true;
                }
                else
                  m.clear();
	      }
	    } // end B.front()!=0
	  } // end B.size()==S+1
	}
      } // end optimization with Hankel system
      tmp=vector<int>(S);
      tmp[S-1]=1;
      vector< vector<int> > K; K.reserve(S+1+d);
      K.push_back(tmp);
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " rur start v<-M*v\n";
      for (int i=0;i<S;++i){
	multmod_positive(mults,multv,tmp,p,tmp1); 
	tmp.swap(tmp1);
	K.push_back(tmp);
      }
      // append Kxi
      for (int i=0;i<d;++i)
	K.push_back(Kxi[i]);
      tran_vect_vector_int(K,tmpm); K.swap(tmpm);  
      vector< vector<int> > Ker;
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " begin rur ker" << '\n';
      if (!mker(K,Ker,p) || Ker.empty() )
	return false;
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << " end rur ker" << '\n';
      vector_int2vecteur(Ker.front(),m);
      reverse(m.begin(),m.end());
      m=trim(m,0);
      // Ker->M
      vectvector_int2vecteur(Ker,M);
      if (m.size()>S+1) return false;
      if (debug_infolevel>2)
	CERR << "Minpoly for " << s << ":" << m << '\n';
      return true;
#endif
      // s^i is obtained by multiplying mults by the coordinates of s^[i-1]
      tmpv[0]=makemod(0,p);
      tmpv[S-1]=1;
      M.push_back(tmpv);
      tmp=vector<int>(S);
      tmp[S-1]=1;
      if (debug_infolevel>1)
	CERR << CLOCK()*1e-6 << "rur start v<-M*v\n";
      for (int i=0;i<S;++i){
	multmod_positive(mults,multv,tmp,p,tmp1); 
	tmp.swap(tmp1);
	vector_int2vecteur(tmp,tmpv);
	M.push_back(tmpv);
      }
      done=true;
      //chk=M; done=false;
    }
    if (!done){ // this code is active only to check optimizations above
      M.clear();
      // set th i-th row of M with coordinates of s^i reduced/gbmod in terms of lm
      vecteur tmp(S);
      tmp[0]=makemod(0,p);
      tmp[S-1]=1;
      M.push_back(tmp);
      polymod<tdeg_t,modint_t> cur(s);
      for (unsigned i=1;i<=lm.coord.size();++i){
	reducesmallmod(cur,gbmod,G,-1,p,TMP1,false);
	// get coordinates of cur in tmp (make them mod p)
	rur_coordinates(cur,lm,tmp);
	M.push_back(tmp);
	// multiply cur and s
	rur_mult(cur,s,p,TMP1);
	cur.coord.swap(TMP1.coord);
      }
    }
#if 1
    if (!chk.empty() && !is_zero(smod(chk-M,p)))
      CERR << "bug\n" ;
    // add coordinates to avoid a separate linsolve with the same matrix
    matrice N(M);
    M.pop_back(); // remove the last one (for further computations, assuming max rank)
    polymod<tdeg_t,modint_t> si(order,dim);
    int d=rur_dim(dim,order);
    vecteur tmp(lm.coord.size());
    polymod<tdeg_t,modint_t> one(order,dim);
    one.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,0));
    for (unsigned i=0;int(i)<d;++i){
      index_t l(dim);
      l[i]=1;
      smallshift(one.coord,tdeg_t(l,order),si.coord);
      reducesmallmod(si,gbmod,G,-1,p,TMP1,false);
      // get coordinates of cur in tmp (make them mod p)
      rur_coordinates(si,lm,tmp);
      N.push_back(tmp);
    }
    N=mtran(N);
    vecteur K;
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin rur ker" << '\n';
    if (!mker(N,K,1,context0) || K.empty() || K.front().type!=_VECT)
      return false;
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " end rur ker" << '\n';
    m=*K.front()._VECTptr;
    rur_cleanmod(m);
    reverse(m.begin(),m.end());
    m=trim(m,0);
    K.swap(M);
    if (m.size()>S+1) return false;
    if (debug_infolevel>2)
      CERR << "Minpoly for " << s << ":" << m << '\n';
    return true;
#else
    if (!chk.empty() && !is_zero(smod(chk-M,p)))
      CERR << "bug\n" ;
    matrice N(M);
    M.pop_back(); // remove the last one (for further computations, assuming max rank)
    if (!N.empty() && !N.front()._VECTptr->empty()) N=mtran(N);
    vecteur K;
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " begin rur ker" << '\n';
    if (!mker(N,K,1,context0) || K.empty() || K.front().type!=_VECT)
      return false;
    if (debug_infolevel>1)
      CERR << CLOCK()*1e-6 << " end rur ker" << '\n';
    m=*K.front()._VECTptr;
    for (unsigned i=0;i<m.size();++i){
      if (m[i].type==_MOD)
	m[i]=*m[i]._MODptr;
    }
    reverse(m.begin(),m.end());
    m=trim(m,0);
    if (debug_infolevel>2)
      CERR << "Minpoly for " << s << " degree " << m.size() << " :" << m << '\n';
    return true;
#endif
  }

  template<class tdeg_t,class modint_t>
  void rur_convert_univariate(const vecteur & v,int varno,polymod<tdeg_t,modint_t> & tmp){
    int vs=int(v.size());
    order_t order=tmp.order;
    tmp.coord.clear();
    index_t l(tmp.dim);
    for (unsigned j=0;int(j)<vs;++j){
      l[varno]=vs-1-j;
      if (v[j].val)
	tmp.coord.push_back(T_unsigned<modint_t,tdeg_t>(v[j].val,tdeg_t(index_m(l),order)));
    }
  }

  template<class tdeg_t,class modint_t>
  void rur_convert_univariate(const vector<int> & v,int varno,polymod<tdeg_t,modint_t> & tmp){
    int vs=int(v.size());
    order_t order=tmp.order;
    tmp.coord.clear();
    index_t l(tmp.dim);
    for (unsigned j=0;int(j)<vs;++j){
      l[varno]=vs-1-j;
      if (v[j])
	tmp.coord.push_back(T_unsigned<modint_t,tdeg_t>(v[j],tdeg_t(index_m(l),order)));
    }
  }

  // if radical==-1, shrink the ideal to radical part
  // if radical==1, the ideal is already radical
  // if radical==0, also tries with radical ideal
  // find a separating element, given the groebner basis and the list of leading
  // monomials of a basis of the quotient ideal
  // if true, then separating element is s
  // and s has m as minimal polynomial,
  // M is the list of rows coordinates of powers of s in lm
  // or is already the rur (computed by using Hankel matrices)
  // (cf. rur_linsolve)
  template<class tdeg_t,class modint_t,class modint_t2>
  bool rur_separate(vectpolymod<tdeg_t,modint_t> & gbmod,polymod<tdeg_t,modint_t> & lm,modint_t p,polymod<tdeg_t,modint_t> & s,vector<int> * initsep,vecteur & m,matrice & M,int radical){
    order_t order=lm.order;
    int dim=lm.dim,d=rur_dim(dim,order);
    s.order=order; s.dim=dim;
    vecteur minp(d);
    if (initsep && !initsep->empty()){
      s.coord.clear(); m.clear(); M.clear();
      for (unsigned i=0;int(i)<d;++i){
        index_t l(dim);
        l[i]=1;
        int r1=(*initsep)[i];
        if (r1) //  && (essai!=testall1 || i!=d-1) )
          s.coord.push_back(T_unsigned<modint_t,tdeg_t>(r1,tdeg_t(l,order)));
      }
      if (!rur_minpoly<tdeg_t,modint_t,modint_t2>(gbmod,lm,s,p,m,M))
        return false;
      if (m.size()==lm.coord.size()+1)
        return true;
      return false;
    }
    // first try coordinates
    for (int i=d-1;i>=0;--i){
      s.coord.clear(); m.clear(); M.clear();
      index_t l(dim);
      l[i]=1;
      s.coord.push_back(T_unsigned<modint_t,tdeg_t>(1,tdeg_t(l,order)));
      if (!rur_minpoly<tdeg_t,modint_t,modint_t2>(gbmod,lm,s,p,m,M))
	return false;
      if (m.size()==lm.coord.size()+1)
	return true;
      // keep m in order to shrink to the radical ideal if separation fails
      if (radical<=0)
	minp[i]=m;
    }
    bool tryseparate=radical!=-1;
    environment env;
    env.modulo=p;
    env.moduloon=true;
    if (radical==0){
      // additional check for non sqrfree minp,
      // for solve([3*x^2-3*z^2,3*y^2-3*z^2,x*y+y*z+x*z-x*y*z],[x,y,z]);
      for (int i=0;i<d;++i){
	if (minp[i].type!=_VECT)
	  continue;
	m=*minp[i]._VECTptr;
	if (m.empty())
	  continue;
	vecteur m1=derivative(m,&env);
	m1=gcd(m,m1,&env);
	if (m1.size()>1){
	  tryseparate=false;
	  break;
	}
      }
    }
    // now try a random small integer linear combination
    if (tryseparate){
      // 6 july 2020: # of tries 40->100 for 
      // eqs:=[c^2 - 3, -a^14 + 85/6*a^12 - 13465/144*a^10 + 54523/144*a^8 - 20819/18*a^6 + 8831/3*a^4 - 7384*a^2 + 10800, -b^14 + 59/4*b^12 - 459/4*b^10 + 8159/16*b^8 - 11777/8*b^6 + 40395/16*b^4 - 10971/4*b^2 + 3267, -1/599040*(53136*b^14 - 692236*b^12 + 4886796*b^10 - 20593959*b^8 + 57747314*b^6 - 116274195*b^4 + 186055404*b^2 - 162887472)*(1008*a^13 - 12240*a^11 + 67325*a^9 - 218092*a^7 + 499656*a^5 - 847776*a^3 + 1063296*a) - 1/112320*(111024*a^14 - 1310760*a^12 + 7843395*a^10 - 35101603*a^8 + 158038072*a^6 - 630801328*a^4 + 1561088256*a^2 - 1489593600)*(56*b^13 - 708*b^11 + 4590*b^9 - 16318*b^7 + 35331*b^5 - 40395*b^3 + 21942*b)];
      // pour j de 1 jusque 100 faire gb:=gbasis(eqs,[c,a,b],rur);fpour;
      // 40 was insufficient for the 11th gbasis rur computation
      unsigned essai=0;
      int testall1=gbmod.size()<=dim+5?0:rur_separate_max_tries/8;
      for (;essai<rur_separate_max_tries;++essai){
	if (debug_infolevel)
	  CERR << CLOCK()*1e-6 << " rur separate non monomial attempt " << essai << '\n';	  
	s.coord.clear(); m.clear(); M.clear();
	int n=(3+2*((essai*essai)/5));
	for (unsigned i=0;int(i)<d;++i){
	  index_t l(dim);
	  l[i]=1;
	  int r1=1;
          if (essai!=testall1) //  && essai!=testall1+1)
            r1=int((std_rand()/double(RAND_MAX)-0.5)*n); // try with the sum of all 
	  if (r1) //  && (essai!=testall1 || i!=d-1) )
	    s.coord.push_back(T_unsigned<modint_t,tdeg_t>(r1,tdeg_t(l,order)));
	}
	if (s.coord.size()<2) // monomials already done
	  continue;
	if (!rur_minpoly<tdeg_t,modint_t,modint_t2>(gbmod,lm,s,p,m,M))
	  return false;
	if (m.size()==lm.coord.size()+1)
	  return true;      
      }
      if (radical==1)
	return false;
      if (essai==rur_separate_max_tries){
	gensizeerr("Unable to find a separation form for the RUR computation. Try rur_separate_max_tries(n) with n larger than "+print_INT_(rur_separate_max_tries));
	return false;
      }
    }
    // shrink ideal and try again
    bool shrinkit=false;
    for (unsigned i=0;int(i)<d;++i){
      if (minp[i].type!=_VECT)
	continue;
      m=*minp[i]._VECTptr;
      if (m.empty())
	continue;
      vecteur m1=derivative(m,&env);
      m1=gcd(m,m1,&env);
      if (m1.size()>1){
	if (debug_infolevel)
	  CERR << "Adding sqrfree part degree " << m1.size()-1 << " " << m1 << " coordinate " << i << '\n';
	m1=operator_div(m,m1,&env); // m1 is the square free part
	polymod<tdeg_t,modint_t> m1mod(order,dim);
	rur_convert_univariate(m1,i,m1mod);
	unsigned j;
	for (j=0;j<gbmod.size();++j){
	  if (tdeg_t_greater(m1mod.coord.front().u,gbmod[j].coord.front().u,order))
	    break;
	}
	gbmod.insert(gbmod.begin()+j,m1mod);
	shrinkit=true;
      }
    }
    if (!shrinkit)
      return false;
    vector<unsigned> G;
    if (!in_gbasisf4buchbergermod<tdeg_t,modint_t>(gbmod,unsigned(gbmod.size()),G,p,/* totdeg */ true,0,0,true))
      return false;
    vectpolymod<tdeg_t,modint_t> newgb;
    for (unsigned i=0;i<G.size();++i){
      if (!gbmod[G[i]].coord.empty())
	newgb.push_back(gbmod[G[i]]);
    }
    newgb.swap(gbmod);
    lm.coord.clear();
    if (rur_quotient_ideal_dimension(gbmod,lm)<0)
      return false;
    if (radical==-1)
      return true;
    return rur_separate<tdeg_t,modint_t,modint_t2>(gbmod,lm,p,s,initsep,m,M,1);
  }

  template<class tdeg_t,class modint_t>
  bool rur_convert(const vecteur & v,const polymod<tdeg_t,modint_t> & lm,polymod<tdeg_t,modint_t> & res){
    res.coord.clear();
    res.order=lm.order; res.dim=lm.dim;
    if (v.size()>lm.coord.size())
      return false;
    for (unsigned i=0;i<v.size();++i){
      gen coeff=v[i];
      if (!is_zero(coeff))
	res.coord.push_back(T_unsigned<modint_t,tdeg_t>(coeff.val,lm.coord[i].u));
    }
    return true;
  }


  template<class tdeg_t>
  bool rur_compute(vectpolymod<tdeg_t,mod4int> & gbmod,polymod<tdeg_t,mod4int> & lm,polymod<tdeg_t,mod4int> & lmmodradical,mod4int p,polymod<tdeg_t,modint> & s,vector<int> * initsep,vectpolymod<tdeg_t,mod4int> & rur){
    // FIXME, call 4 modint rur_compute
    return false;
  }

  // set rur to be the list of s, 
  // m the minimal polynomial of s as a polymod<tdeg_t,modint_t> wrt the 1st var
  // and for each coordinate (sqrfree part of m) * coordinate
  // expressed as a polynomial in s (stored in a polymod<tdeg_t,modint_t> wrt 1st var)
  // Current method (not optimal if there are multiplicities)
  // Try each coordinate as a separating form s, then random linear combination
  // If minpoly of s is squarefree and max degree, the ideal is cyclic
  // If minpoly of s is not squarefree, shrink the ideal by adding
  // the squarefree part of the minpoly of s to the ideal
  // This will make the ideal radical but may be too costly.
  //
  // Improvement to be implemented
  // F. Rouillier et al: https://arxiv.org/pdf/2402.07141 (Maple code zds.mpl)
  //
  // check if s is separating on bivariate ideals
  // requires computing minpoly of s, and lex gbasis of bivariate ideals
  // for all coordinates X=X[1] to X[j], find polynomials in s and X in ideal
  // gbasis=(minpoly(s),sum(a[k,i](s)*X^i,i=0..k))
  // gbasis contains may 0 polynomial, so that degree(gbasis[k])==k if non 0
  //
  // Algo 2: check separating for X=X[j]
  // f[0]=sqrfree(minpoly of s)
  // for k=1 to sizeof(gbasis)
  //   if (gbasis[k]==0) continue;
  //   f[k]=f[k-1]/gcd(f[k-1],lcoeff(gbasis[k]))
  //   for i=0 to k-1
  //     if (k*(k-i)/(i+1)*a[k,k]*a[k,i] != a[k,k-1]*a[k,i+1] mod f[k])
  //        return false;
  //     end_if
  //   end_for
  // end_for
  //
  // Algo 3: find bivariate parametrization (for all coord X=X[1]to X[k])
  // n=0, d=0, rho=1, f[0]=sqrfree(f)
  // for k from 1 to sizeof(gbasis)
  //   if (gbasis[k]==0) continue;
  //   f[k] = f[k-1]/gcd(f[k-1],lcoeff(gbasis[k]))
  //   rho *= f[k]
  //   d += k*a[k,k]*rho mod f[0]
  //   n += a[k,k-1]*rho mod f[0]
  // end_for
  //
  // Algo 4: check a candidate separating form and return rur
  // arg: do_check (if false we don't check), s (separating form), gbasis
  // find minpoly(s) and store the list of rref-ed s^k for later computations
  // for each coordinate X[j]
  //   compute a lex gbasis for algo2, using the list above (FGLM algo)
  //   if (do_check) check with algo2, if not return failure and coordinate #
  //   compute rational parametrization then rur for this coordinate
  // return rur
  //
  // Algo 6: run algo 4 first on coordinates, then on a linear combination
  // constructed using the previous candidate and modifying the coeff
  // of the coordinate number that returns the failure
  // if coeff<0 change sign, if coeff>=0 replace by -coeff-1
  //
  // Algo 1: from rational param to rur
  // Arg=(f(s)==0 not necessarily sqrfree, d[k](s)*X[k]+n[k](s)
  // compute F=sqrfree(f), F1=diff(F)
  // for k=1 to dim to rur[k]=-n[k]*inv(d[k] mod F)*F1 mod F
  template<class tdeg_t>
  bool rur_compute(vectpolymod<tdeg_t,modint> & gbmod,polymod<tdeg_t,modint> & lm,polymod<tdeg_t,modint> & lmmodradical,int p,polymod<tdeg_t,modint> & s,vector<int> * initsep,vectpolymod<tdeg_t,modint> & rur){
    vecteur m,M,res;
    int dim=lm.dim;
    order_t order=lm.order;
    if (s.coord.empty()){
      // find separating element
      if (!rur_separate<tdeg_t,modint,modint2>(gbmod,lm,p,s,initsep,m,M,0))
	return false;
    }
    else {
      // if lm!=lmmodradical, ideal is not radical, recompute radical part
      if (!(lm==lmmodradical)){
	polymod<tdeg_t,modint> s1(s.order,s.dim);
	if (!rur_separate<tdeg_t,modint,modint2>(gbmod,lm,p,s1,initsep,m,M,-1))
	  return false;
      }
      // separating element is already known
      if (!rur_minpoly<tdeg_t,modint,modint2>(gbmod,lm,s,p,m,M) || m.size()!=lm.coord.size()+1)
	return false;
    }
    // find the square-free part of m, express it as a polymod<tdeg_t,modint> using M
    environment env;
    env.modulo=p;
    env.moduloon=true;
    vecteur m1=derivative(m,&env);
    m1=gcd(m,m1,&env);
    bool sqf=m1.size()>1;
    m1=operator_div(m,m1,&env); // m1 is the square free part
    if (debug_infolevel && sqf)
      CERR << CLOCK()*1e-6 << " sqrfree mod " << p << " degree " << m1.size()-1 << " " << m1 << '\n';
    vecteur m2=derivative(m1,&env); // m2 is the derivative, prime with m1
    // multiply by m2 at the end
    if (debug_infolevel)
      CERR << CLOCK()*1e-6 << " rur linsolve" << '\n';
    polymod<tdeg_t,modint> one(order,dim);
    one.coord.push_back(T_unsigned<modint,tdeg_t>(1,0));
    if (!rur_linsolve(gbmod,lm,one,M,p,res))
      return false;
    // rur=[separating element,sqrfree part of minpoly,derivative of sqrfree part,
    // derivative of sqrfree part*other coordinates]
    rur.clear();
    rur.push_back(s);
    polymod<tdeg_t,modint> tmp(order,dim);
    rur_convert_univariate(m1,0,tmp);
    rur.push_back(tmp);
    rur_convert_univariate(m2,0,tmp);
    rur.push_back(tmp);
    vector<int> m2i; vecteur2vector_int(m2,0,m2i);
    vector<int> m1i; vecteur2vector_int(m1,0,m1i);
    // convert res to rur
    for (unsigned i=0;i<res.size();++i){
#if 1
      vector<int> v,w,q; 
      vecteur2vector_int(*res[i]._VECTptr,0,v);
      operator_times(v,m2i,p,w);
      DivRem(w,m1i,p,q,v);
      rur_convert_univariate(v,0,tmp);
      rur.push_back(tmp);
      continue;
#endif
      vecteur V(*res[i]._VECTptr),W,Q;
      mulmodpoly(V,m2,&env,W);
      DivRem(W,m1,&env,Q,V);
      rur_convert_univariate(V,0,tmp);
      rur.push_back(tmp);
    }
    return true;
  }

  // returns -1 if lm1 is not contained in lm2 and lm2 is not contained in lm1
  // returns 0 if lm1==lm2
  // returns 1 if lm1 contains lm2
  // returns 2 if lm2 contains lm1
  template<class tdeg_t,class modint_t>
  int rur_compare(polymod<tdeg_t,modint_t> & lm1,polymod<tdeg_t,modint_t> & lm2){
    unsigned s1=unsigned(lm1.coord.size()),s2=unsigned(lm2.coord.size());
    if (s1==s2){
      if (lm1==lm2)
	return 0;
      return -1;
    }
    if (s1>s2){
      unsigned i=0;
      for (unsigned j=0;j<s2;++i,++j){
	for (;i<s1;++i){
	  if (lm1.coord[i].u==lm2.coord[j].u)
	    break;
	}
	if (i==s1)
	  return -1;
      }
      return 1;
    }
    unsigned j=0;
    for (unsigned i=0;i<s1;++i,++j){
      for (;j<s2;++j){
	if (lm1.coord[i].u==lm2.coord[j].u)
	  break;
      }
      if (j==s2)
	return -1;
    }
    return 2;
  }

  /* ******************
     END RUR UTILITIES 
     ****************** */

#ifdef HAVE_LIBPTHREAD
  template<class tdeg_t,class modint_t>
  struct thread_gbasis_t {
    vectpoly8<tdeg_t> * currentptr;
    vectpolymod<tdeg_t,modint_t> resmod,rurv;
    polymod<tdeg_t,modint_t> rurlm,rurlmmodradical,*rurgblmptr,*rurlmptr;
    polymod<tdeg_t,modint> rurs;
    vector<int> * initsep;
    vector< vectpolymod<tdeg_t,modint_t> > * coeffsmodptr;
    vector<unsigned> G;
    modint_t p;
    vector< paire > * reduceto0;
    vector< info_t<tdeg_t,modint_t> > * f4buchberger_info;
    vector< zinfo_t<tdeg_t> > * zf4buchberger_info;
    bool zdata;
    bool eliminate_flag; // if true, for double revlex order returns only the gbasis part made of polynomials that do not depend on variables to eliminate
    bool interred;
    gbasis_param_t gparam;
    int rurinzgbasis;
    int parallel; // max number of parallel threads for 1 modular computation
  };
  
  template<class tdeg_t,class modint_t,class modint_t2>
  void * thread_gbasis(void * ptr_){
    thread_gbasis_t<tdeg_t,modint_t> * ptr=(thread_gbasis_t<tdeg_t,modint_t> *) ptr_;
    ptr->G.clear();
    if (ptr->zdata){
      if (!zgbasisrur<tdeg_t,modint_t,modint_t2>(*ptr->currentptr,ptr->resmod,ptr->G,ptr->p,true,
		   ptr->reduceto0,*ptr->zf4buchberger_info,false,false,ptr->eliminate_flag,true,ptr->parallel,ptr->interred,
                                                 ptr->rurinzgbasis,ptr->rurv,ptr->rurs,ptr->initsep,ptr->rurlm,ptr->rurlmmodradical,ptr->rurgblmptr,ptr->rurlmptr,ptr->gparam,ptr->coeffsmodptr))
	return 0;
      ptr->zdata=0;
    }
    else {
#if 1 // ndef GBASIS_4PRIMES
      if (!in_gbasisf4buchbergermod(*ptr->currentptr,ptr->resmod,ptr->G,ptr->p,true/*totaldeg*/,
				    ptr->reduceto0,ptr->f4buchberger_info,false))
#endif
	return 0;
    }
    return ptr_;
  }
#endif

  template<class tdeg_t>
  bool check_initial_generators(vectpoly8<tdeg_t> & res,const vectpoly8<tdeg_t> & Wi,vector<unsigned> & G,double eps){
    int initial=int(res.size());
    poly8<tdeg_t> tmp0,tmp1,tmp2;
    vectpoly8<tdeg_t> wtmp;
    unsigned j=0,finalchecks=initial;
    if (eps>0)
      finalchecks=giacmin(2*Wi.front().dim,initial);
    if (debug_infolevel)
      CERR << CLOCK()*1e-6 << " begin final check, checking that " << finalchecks << " initial generators belongs to the ideal" << '\n';
    G.resize(Wi.size());
    for (j=0;j<Wi.size();++j)
      G[j]=j;
    vector<bool> Gused(G.size());
    for (j=0;j<finalchecks;++j){
      if (debug_infolevel)
	CERR << "+";
      sort(res[j].coord.begin(),res[j].coord.end(),tdeg_t_sort_t<tdeg_t>(res[j].order));
      reduce(res[j],Wi,G,-1,wtmp,tmp0,tmp1,tmp2,0,&Gused);
      if (!tmp0.coord.empty()){
	break;
      }
      if (debug_infolevel && (j%10==9))
	CERR << j+1 << '\n';
    }
    if (debug_infolevel){
      CERR << '\n' << " Elements used for reduction ";
      for (size_t i=0;i<G.size();++i){
	CERR << (Gused[i]?'+':'-');
      }
      CERR << '\n';
    }
    if (j!=finalchecks){
      if (debug_infolevel){
	CERR << CLOCK()*1e-6 << " final check failure, retrying with another prime " << '\n';
	CERR << "Non-zero remainder " << tmp0 << '\n';
	CERR << "checking res[j], " << j << "<" << initial << '\n';
	CERR << "res[j]=" << res[j] << '\n';
	CERR << "basis candidate " << Wi << '\n';
      }
      return false;
    }
    return true;
  }

  template<class tdeg_t>
  void convert_univariate(const poly8<tdeg_t> & p,modpoly & P){
    P.clear(); if (p.coord.empty()) return;
    unsigned char pdim=p.dim; const order_t order={(short) _REVLEX_ORDER,pdim};
    int deg=p.coord.front().u.total_degree(order);
    P.resize(deg+1);
    for (unsigned i=0;i<p.coord.size();++i){
      int d=p.coord[i].u.total_degree(order);
      P[deg-d]=p.coord[i].g;
    }
  }

  template<class tdeg_t>
  struct rur_certify_t {
    const vectpoly8<tdeg_t> * syst;
    const modpoly * minp;
    const modpoly * dminp; 
    const vector<modpoly> * v; 
    const gen * dminpden; 
    const vecteur * vden;
    vector<int> chk_index;
    order_t order;
    int dim;
    bool ans;
    int threadno;
    const context * contextptr;
  };

  modpoly free_copy(const modpoly & v){
    modpoly res(v.size());
    for (int i=0;i<v.size();++i)
      res[i]=v[i].type==_ZINT?*v[i]._ZINTptr:v[i];
    return res;
  }
  
  template<class tdeg_t>
  void * thread_rur_certify(void * ptr)  {
    rur_certify_t<tdeg_t> * Rptr=(rur_certify_t<tdeg_t> *) ptr;
    const vectpoly8<tdeg_t> & syst =*Rptr->syst;
    modpoly minp=free_copy(*Rptr->minp);
    modpoly dminp=free_copy(*Rptr->dminp);
    modpoly tmp,rem;
    vector<modpoly> v(Rptr->v->size());
    for (int i=0;i<v.size();++i)
      v[i]=free_copy((*Rptr->v)[i]);
    vecteur vden=free_copy(*Rptr->vden);
    const vector<int> & chk_index=Rptr->chk_index;
    const order_t & order=Rptr->order;
    int dim=Rptr->dim,locked=false;
    const context * contextptr=Rptr->contextptr;
    const gen dminpden=*Rptr->dminpden;
    for (int i_=0;i_<chk_index.size();i_++){
      int i=chk_index[i_];
      const poly8<tdeg_t> & cur=syst[i];
      if (cur.coord.empty()) continue;
      int deg=cur.coord.front().u.total_degree(order);
#ifdef HAVE_LIBPTHREAD
      locked=pthread_mutex_trylock(&rur_mutex);
#endif
      if (rur_do_certify>0 && deg>rur_do_certify){
	*logptr(locked?context0:contextptr) << "rur_certify: equation not checked, degree too large " << deg << " run rur_certify(1) to check all equations\n";
	continue;
      }
#ifdef HAVE_LIBPTHREAD
      if (locked) pthread_mutex_unlock(&rur_mutex);	
#endif
      if (Rptr->threadno==0 && 
	  debug_infolevel) *logptr(contextptr) << clock_realtime() << " rur_certify checking equation "<< i << " degree " << deg << "\n";
      modpoly sum; gen sumden(1);
      for (int j=0;j<cur.coord.size();++j){
	modpoly prod(1,1),tmp; gen prodden(1);
	index_t idx; get_index(cur.coord[j].u,idx,order,dim);
	int curdeg=cur.coord[j].u.total_degree(order); // or idx.total_deg
	vector<modpoly *> vp; vp.reserve(deg);
	for (int k=0;k<dim;++k){
	  int alphak=idx[k];
	  for (int l=0;l<alphak;++l){
	    prodden = prodden*vden[k];
	    vp.push_back(&v[k]);
	  }
	}
	for (int l=0;l<deg-curdeg;++l){
	  prodden = prodden*dminpden;
	  vp.push_back(&dminp);
	}
	mulmodpoly(vp,0,prod,Rptr->threadno==0?debug_infolevel:0);
	mulmodpoly(prod,cur.coord[j].g,0,prod);
	// sum/sumden = sum/sumden+prod/prodden
	// = (sum*(prodden/g)+prod*(sumden/g)) / (g*prodden/g*sumden/g)
	gen g=simplify3(sumden,prodden);
	mulmodpoly(sum,prodden,0,sum);
	mulmodpoly(prod,sumden,0,prod);
	addmodpoly(sum,prod,0,tmp); sum.swap(tmp);
	sumden=g*sumden*prodden;
      }
      // remainder
      if (!DivRem(sum,minp,0,tmp,rem,true) || !rem.empty()){
	Rptr->ans=false;
#ifdef HAVE_LIBPTHREAD
	locked=pthread_mutex_trylock(&rur_mutex);
#endif
	*logptr(locked?context0:contextptr) << "rur_certify failure equation " << i << "\n";
#ifdef HAVE_LIBPTHREAD
	if (locked) pthread_mutex_unlock(&rur_mutex);	
#endif
	return ptr;
      }
#ifdef HAVE_LIBPTHREAD
      locked=pthread_mutex_trylock(&rur_mutex);
#endif
      if (debug_infolevel)
        *logptr(contextptr) << clock_realtime() << " rur_certify equation "<< i << " degree " << deg << " check success.\n";
    }
    Rptr->ans=true;
    return ptr;
  }

  template<class tdeg_t>
  bool rur_certify(const vectpoly8<tdeg_t> & syst,vectpoly8<tdeg_t> & val,int gbshift,GIAC_CONTEXT){
    if (rur_do_certify<0) return true;
    // rur final check could be performed by replacing
    // val[gbshift+3..end]/val[gbshift+2] 
    // in the initial syst system variables 
    // and check if it's 0 mod val[gbshift+1]
    // u.total_degree(order), get_index(u,index_t,order,dim)
    if (syst.empty()) return true;
    unsigned char pdim=syst[0].dim; const order_t order={(short) _REVLEX_ORDER,pdim};
    int dim=val.size()-gbshift-3;
    modpoly minp,dminp,rem,tmp; vector<modpoly> v(dim); gen minpden,dminpden; vecteur vden(dim);
    cpureal_t t1=clock_realtime();
    size_t nm=0;
    for (int i=0;i<syst.size();++i){
      nm += syst[i].coord.size();
    }
    if (debug_infolevel)
      *logptr(contextptr) << clock_realtime() << " rur_certify monomials number " << nm << '\n';
    if (debug_infolevel) CERR << t1 << " rur_certify convert univariate\n";
    convert_univariate(val[gbshift+1],minp); lcmdeno(minp,minpden,context0);
    convert_univariate(val[gbshift+2],dminp); lcmdeno(dminp,dminpden,context0);
    for (int i=0;i<dim;i++){
      convert_univariate(val[gbshift+i+3],v[i]);
      lcmdeno(v[i],vden[i],context0);
    }
#ifdef HAVE_LIBPTHREAD
    int nthreads=threads_allowed?giacmin(threads,MAXNTHREADS):1;
    if (nthreads>1){
      if (nthreads>rur_certify_maxthreads) nthreads=rur_certify_maxthreads; // don't use too much memory
      if (debug_infolevel)
        *logptr(contextptr) << "rur_certify: multi-thread check, info displayed on may miss some threads info. Threads in use: " << nthreads << "\n";
      pthread_t tab[MAXNTHREADS];
      vector< rur_certify_t<tdeg_t> > rur_certify_param; rur_certify_param.reserve(nthreads);
      for (int j=0;j<nthreads;++j){
	vector<int> chk_index;
	for (int k=j;k<syst.size();k+=nthreads)
	  chk_index.push_back(k);
	rur_certify_t<tdeg_t> cur={&syst,&minp,&dminp,&v,&dminpden,&vden,chk_index,order,dim,true,j,contextptr};
	rur_certify_param.push_back(cur);
	bool res=true;
	if (j<nthreads-1)
	  res=pthread_create(&tab[j],(pthread_attr_t *) NULL,thread_rur_certify<tdeg_t>,(void *) &rur_certify_param[j]);
	if (res)
	  thread_rur_certify<tdeg_t>((void *)&rur_certify_param[j]);
      }
      bool ans=true;
      void * threadretval[MAXNTHREADS];
      for (int j=0;j<nthreads;++j){
	threadretval[j]=&threadretval; // non-0 initialization
	if (j<nthreads-1)
	  pthread_join(tab[j],&threadretval[j]);
	ans=ans && rur_certify_param[j].ans;
      }
      if (debug_infolevel)
        *logptr(contextptr) << "end rur_certify, certification time " << clock_realtime()-t1 << "\n";
      return ans;
    }
#endif
    for (int i=0;i<syst.size();i++){
      const poly8<tdeg_t> & cur=syst[i];
      if (cur.coord.empty()) continue;
      int deg=cur.coord.front().u.total_degree(order);
      if (debug_infolevel) *logptr(contextptr) << CLOCK()*1e-6 << " rur_certify cheking equation "<< i << " degree " << deg << "\n";
      modpoly sum; gen sumden(1);
      for (int j=0;j<cur.coord.size();++j){
	modpoly prod(1,1),tmp; gen prodden(1);
	index_t idx; get_index(cur.coord[j].u,idx,order,dim);
	int curdeg=cur.coord[j].u.total_degree(order); // or idx.total_deg
	vector<modpoly *> vp; vp.reserve(deg);
	for (int k=0;k<dim;++k){
	  int alphak=idx[k];
	  for (int l=0;l<alphak;++l){
	    prodden = prodden*vden[k];
	    vp.push_back(&v[k]);
	  }
	}
	for (int l=0;l<deg-curdeg;++l){
	  prodden = prodden*dminpden;
	  vp.push_back(&dminp);
	}
	mulmodpoly(vp,0,prod,debug_infolevel);
	mulmodpoly(prod,cur.coord[j].g,0,prod);
	// sum/sumden = sum/sumden+prod/prodden
	// = (sum*(prodden/g)+prod*(sumden/g)) / (g*prodden/g*sumden/g)
	gen g=simplify3(sumden,prodden);
	mulmodpoly(sum,prodden,0,sum);
	mulmodpoly(prod,sumden,0,prod);
	addmodpoly(sum,prod,0,tmp); sum.swap(tmp);
	sumden=g*sumden*prodden;
      }
      // remainder
      if (!DivRem(sum,minp,0,tmp,rem,true))
	return false;
      if (!rem.empty())
	return false;
      if (debug_infolevel)
        *logptr(contextptr) << clock_realtime() << " rur_certify equation "<< i << " degree " << deg << " check success\n";
    }
    return true;
  }

#ifdef GBASIS_4PRIMES

  typedef mod4int qmodint;
  typedef mod4int2 qmodint2;
  qmodint prevprime_qmodint(qmodint & p4,const gen & llcm){
    int p=p4.tab[sizeof(mod4int)/sizeof(modint)-1];
    for (;;){
      p=prevprime(p-1).val;
      if (!is_zero(llcm % p))
	break;
    }
    mod4int res={p};
    for (int i=1;i<sizeof(mod4int)/sizeof(modint);++i){
      for (;;){
	p=prevprime(p-1).val;
	if (!is_zero(llcm % p))
	  break;
      }
      res.tab[i]=p;
    }
    return p4=res;
  }
  template<class tdeg_t> void copy(const polymod<tdeg_t,modint> & src, polymod<tdeg_t,qmodint> & target){
    convert(src,target);
  }

  inline int getint(qmodint p, int pos){ return p.tab[pos]; }

#else // GBASIS_4PRIMES

  typedef modint qmodint;
  typedef modint2 qmodint2;
  qmodint prevprime_qmodint(qmodint & p,const gen & llcm){
    // find a prime not dividing llcm (the lcm of the leading coeffs of the initial basis)
    for (;;){
      p=prevprime(p-1).val;
      if (!is_zero(llcm % p))
	break;
    }
    return p;
  }
  inline int getint(qmodint p, int pos){ return p; }
  template<class tdeg_t> void copy(const polymod<tdeg_t,modint> & src, polymod<tdeg_t,qmodint> & target){
    target=src;
  }

#endif // GBASIS_4PRIMES

  // return 0 (failure), 1 (success), -1: parts of the gbasis reconstructed
  template<class tdeg_t,class qmodint_t,class qmodint_t2>
  int in_mod_gbasis(vectpoly8<tdeg_t> & res,bool modularcheck,bool zdata,int & rur,GIAC_CONTEXT,gbasis_param_t gbasis_par,int gbasis_logz_age,vector< vectpoly8<tdeg_t> > * coeffsmodptr=0){
    gen llcm=1;
    for (int i=0;i<res.size();++i){
      const poly8<tdeg_t> & cur=res[i];
      if (!cur.coord.empty())
	llcm=lcm(llcm,cur.coord.front().g);
    }
    if (debug_infolevel)
      CERR << "Lcm of leading coefficients of initial generators " << llcm << "\n";
    cpureal_t init_time=clock_realtime();
    if (debug_infolevel)
      CERR << CLOCK()*1e-6 << " modular gbasis algorithm start, mem " << memory_usage() << '\n';
    if (coeffsmodptr && !zdata)
      return 0;
    bool & eliminate_flag=gbasis_par.eliminate_flag;
    bool interred=gbasis_logz_age==0; // final interreduce
    if (interred)
      interred=gbasis_par.interred;
    unsigned initial=unsigned(res.size());
    double eps=proba_epsilon(contextptr); int rechecked=0;
    order_t order={0,0};
    bool multithread_enabled=true;
    // multithread was disabled for more than 14 vars because otherwise
    // threads:=2; n:=9;P:=mul(1+x[j]*t,j=0..n-1);
    // X:=[seq(x[j],j=0..n-1)];
    // S:=seq(p[j]-coeff(P,t,j), j=1..n-1);
    //  N:=sum(x[j]^(n-1),j=0..n-1);
    // I:=[N,S]:;eliminate(I,X)
    // segfaults and valgrind does not help... seems to work now but not z8
    for (unsigned i=0;i<res.size();++i){
      const poly8<tdeg_t> & P=res[i];
      if (multithread_enabled && !P.coord.empty()){
#ifdef ATOMIC
#else
	multithread_enabled=!P.coord.front().u.vars64();
#endif
      }
      order=P.order;
      for (unsigned j=0;j<P.coord.size();++j){
	if (!is_integer(P.coord[j].g)) // improve: accept complex numbers
	  return 0;
      }
    }
    if (order.o!=_REVLEX_ORDER && order.o!=_3VAR_ORDER && order.o!=_7VAR_ORDER && order.o!=_11VAR_ORDER && order.o!=_16VAR_ORDER && order.o!=_32VAR_ORDER && order.o!=_48VAR_ORDER && order.o!=_64VAR_ORDER)
      return 0;
    vectpoly8<tdeg_t> toreinject;
    if (gbasis_par.reinject_begin>=0 && gbasis_par.reinject_end>gbasis_par.reinject_begin){
      toreinject=res; //vectpoly8<tdeg_t>(res.begin()+gbasis_par.reinject_begin,res.begin()+gbasis_par.reinject_end);
      if (gbasis_par.reinject_for_calc>0 && gbasis_par.reinject_for_calc<res.size())
	res.resize(gbasis_par.reinject_for_calc);
      sort(res.begin(),res.end(),tripolymod_tri<poly8<tdeg_t> >(gbasis_logz_age));
    }
    // if (order!=_REVLEX_ORDER) zdata=false;
    vectpoly8<tdeg_t> current,current_orig,current_gbasis,vtmp,afewpolys;
    vectpolymod<tdeg_t,qmodint_t> resmod;
    vectpolymod<tdeg_t,modint> gbmod;
    poly8<tdeg_t> poly8tmp;
#if defined(EMCC) || defined(EMCC2)
    // use smaller primes
    int pstart=94906249-_floor(giac_rand(contextptr)/32e3,contextptr).val;
    // gen p=(1<<24)-_floor(giac_rand(contextptr)/32e3,contextptr);
#else
    int pstart=(1<<29)-_floor(giac_rand(contextptr)/1e3,contextptr).val;
#endif
#ifdef GBASIS_4PRIMES
    qmodint_t p_qmodint=mkmod4int(pstart);
#else
    qmodint_t p_qmodint=pstart;
#endif
    int pcur; 
    // unless we are unlucky these lists should contain only 1 element
    vector< vectpoly8<tdeg_t> > V; // list of (chinrem reconstructed) modular groebner basis
    vector< vectpoly8<tdeg_t> > W; // list of rational reconstructed groebner basis
    vector< vectpoly8<tdeg_t> > Wlast;
    int dim=0; vectpoly8<tdeg_t> Wrur; // rur reconstruction part
    vecteur P; // list of associate (product of) modulo
    // variables for rational univar. reconstr.
    polymod<tdeg_t,qmodint_t> lmmod,lmmodradical,prevgblm,mainthrurlm,mainthrurlmsave,mainthrurlmmodradical,mainthrurgblm;
    vectpolymod<tdeg_t,qmodint_t> mainthrurv; 
    polymod<tdeg_t,modint> rurs,cur_gblm,prev_gblm,zlmmod,zlmmodradical;
    vectpolymod<tdeg_t,modint> rurv;
    // zrur is !=0 if rur computation was already done in zgbasis
    int prevrqi; int zrur=0,rurinzgbasis=0,mainthrurinzgbasis=0;
    // environment env;
    // env.moduloon=true;
    vector<unsigned> G;
    vector< paire > reduceto0;
    vector< info_t<tdeg_t,qmodint_t> > f4buchberger_info;
    f4buchberger_info.reserve(GBASISF4_MAXITER);
    vector<zinfo_t<tdeg_t> > zf4buchberger_info;
    zf4buchberger_info.reserve(GBASISF4_MAXITER);
    mpz_t zu,zd,zu1,zd1,zabsd1,zsqrtm,zq,zur,zr,ztmp;
    mpz_init(zu);
    mpz_init(zd);
    mpz_init(zu1);
    mpz_init(zd1);
    mpz_init(zabsd1);
    mpz_init(zsqrtm);
    mpz_init(zq);
    mpz_init(zur);
    mpz_init(zr);
    mpz_init(ztmp);
    bool ok=true;
#ifdef HAVE_LIBPTHREAD
    int nthreads=(threads_allowed && multithread_enabled)?giacmin(threads,MAXNTHREADS):1,th,parallel=1;
    pthread_t tab[MAXNTHREADS];
    thread_gbasis_t<tdeg_t,qmodint_t> gbasis_param[MAXNTHREADS];
#else
    int nthreads=1,th,parallel=1;
#endif
    bool rur_gbasis=rur_do_gbasis>=0 || gbasis_par.gbasis;
    bool chk_initial_generator=true;
    // for more than 2 threads, real time is currently better without
    // reason might be that the gbasis is large, reduction mod p for
    // all threads has bad cache performances?
    // IMPROVE 1: compute resmod for all threads simult in main thread
    // IMPROVE 2: check whether the rur stabilizes before the gbasis!
    int initgensize=0; // number of initial generators (if computing coeffs)
    ulonglong nmonoms; // number of monoms in gbasis
    int recon_n2=-1,recon_n1=-1,recon_n0=-1,recon_added=0,recon_count=0,gbasis_size=-1,jpos_start=-1; // reconstr. gbasis element number history
    double augmentgbasis=gbasis_reinject_ratio,prevreconpart=1.0,time1strun=-1.0,time2ndrun=-1.0; current_orig=res; current_gbasis=res;
    int primecount=0;
    // if the ratio of reconstructed is more than augmentgbasis,
    // we clear info and add reconstruction to the gbasis
    for (int count=0;ok;++count,++recon_count){
      if (count==0 || nthreads==1 || (zdata && augmentgbasis && reduceto0.empty())){
	th=0;
	parallel=nthreads;
      }
      else {
	unsigned sp=simult_primes;
	if (count>=simult_primes_seuil2)
	  sp=simult_primes2;
	if (count>=simult_primes_seuil3)
	  sp=simult_primes3;
	th=giacmin(nthreads-1,sp-1); // no more than simult_primes 
	th=giacmin(th,MAXNTHREADS-1);
	parallel=nthreads/(th+1);
      }
      int effth=sizeof(qmodint_t)/sizeof(modint)*(th+1);
      /* *************************
       *  FIND PRIMES AND COMPUTE
       ****************************  */
      // FIXME we should avoid primes that divide one of leading coeff of current_gbasis
      // compute gbasis mod p 
      // env.modulo=p;
      if (th==0) rurinzgbasis=0;
      copy(zlmmodradical,lmmodradical);
      mainthrurinzgbasis=lmmodradical.coord.empty()?0:rur;
      mainthrurlmmodradical=lmmodradical;
      vector< vector< vectpolymod<tdeg_t,qmodint_t> > > gbasiscoeffv(th+1);
#ifdef HAVE_LIBPTHREAD
      vector<qmodint_t> pthread_p(th+1); vector< vector<polymod<tdeg_t,qmodint_t> > *> pthread_mod(th+1);
      for (unsigned j=0;j<th;++j){
	gbasis_param[j].currentptr=&current_gbasis;
        prevprime_qmodint(p_qmodint,llcm);
	gbasis_param[j].p=p_qmodint; 
	gbasis_param[j].reduceto0=&reduceto0;
	gbasis_param[j].f4buchberger_info=&f4buchberger_info;
	gbasis_param[j].zf4buchberger_info=&zf4buchberger_info;
	gbasis_param[j].zdata=zdata;
	gbasis_param[j].eliminate_flag=eliminate_flag;
	gbasis_param[j].parallel=parallel;
	gbasis_param[j].interred=interred;
	gbasis_param[j].rurinzgbasis=lmmodradical.coord.empty()?0:rur;
	gbasis_param[j].rurlm=polymod<tdeg_t,qmodint_t>(lmmodradical.order,lmmodradical.dim);
	gbasis_param[j].rurlmmodradical=lmmodradical;
	gbasis_param[j].rurs=rurs;
	gbasis_param[j].initsep=&gbasis_par.initsep;
	gbasis_param[j].rurgblmptr=&mainthrurgblm;
	gbasis_param[j].rurlmptr=&mainthrurlmsave;
	gbasis_param[j].gparam=gbasis_par;
	gbasis_param[j].coeffsmodptr=coeffsmodptr?&gbasiscoeffv[j]:0;
	if (count==1)
	  gbasis_param[j].resmod.reserve(resmod.size());
	pthread_p[j]=p_qmodint; pthread_mod[j]=&gbasis_param[j].resmod;
      }
      p_qmodint=prevprime_qmodint(p_qmodint,llcm); 
      pthread_p[th]=p_qmodint; pthread_mod[th]=&resmod;
      for (unsigned j=0;j<th;++j){      
	bool res=true;
	// CERR << "write " << j << " " << p << '\n';
	res=pthread_create(&tab[j],(pthread_attr_t *) NULL,thread_gbasis<tdeg_t,qmodint_t,qmodint_t2>,(void *) &gbasis_param[j]);
	if (res)
	  thread_gbasis<tdeg_t,qmodint_t,qmodint_t2>((void *)&gbasis_param[j]);
      }
#else // PTHREAD
      p_qmodint=prevprime_qmodint(p_qmodint,llcm); 
#endif // PTHREAD
      if (!zdata) current=current_gbasis;
      G.clear();
      double t_0=CLOCK()*1e-6;
#if !defined KHICAS && !defined SDL_KHICAS
      if (debug_infolevel)
	CERR << std::setprecision(15) << clock_realtime() << " begin computing basis modulo " << p_qmodint << " batch/threads " << th+1 << "/" << parallel << '\n';
#endif
      // CERR << "write " << th << " " << p << '\n';
#ifdef GBASISF4_BUCHBERGER 
      if (zdata){
	if (!zgbasisrur<tdeg_t,qmodint_t,qmodint_t2>(current_gbasis,resmod,G,p_qmodint,true,&reduceto0,zf4buchberger_info,false,false,eliminate_flag,true,parallel,interred,mainthrurinzgbasis,mainthrurv,rurs,&gbasis_par.initsep,mainthrurlm,mainthrurlmmodradical,&mainthrurgblm,&mainthrurlmsave,gbasis_par,coeffsmodptr?&gbasiscoeffv[th]:0)){
	  if (augmentgbasis>0) 
	    augmentgbasis=2;
	  reduceto0.clear();
	  zf4buchberger_info.clear();
	  zf4buchberger_info.reserve(4*zf4buchberger_info.capacity());
	  G.clear();
	  if (!zgbasisrur<tdeg_t,qmodint_t,qmodint_t2>(current_gbasis,resmod,G,p_qmodint,true/*totaldeg*/,&reduceto0,zf4buchberger_info,false,false,eliminate_flag,true,parallel,interred,mainthrurinzgbasis,mainthrurv,rurs,&gbasis_par.initsep,mainthrurlm,mainthrurlmmodradical,&mainthrurgblm,&mainthrurlmsave,gbasis_par,coeffsmodptr?&gbasiscoeffv[th]:0)){
	    ok=false;
	    break;
	  }
	}
      }
      else {
#if 0 // def GBASIS_4PRIMES
        return 0;
#else
	resmod.clear();
	if (!in_gbasisf4buchbergermod(current,resmod,G,p_qmodint,true/*totaldeg*/,
				      //		  0,0
				      &reduceto0,&f4buchberger_info,
#if 1
				      false /* not useful */
#else
				      (count==1) /* recompute R and quo at 2nd iteration*/
#endif
				      )){
	  // retry 
	  reduceto0.clear();
	  f4buchberger_info.clear(); G.clear();
	  if (!in_gbasisf4buchbergermod(current,resmod,G,p_qmodint,true/*totaldeg*/,&reduceto0,&f4buchberger_info,false)){
	    ok=false;
	    break;
	  }
	  reduceto0.clear();
	  f4buchberger_info.clear();
	}
#endif // GBASIS_4PRIMES
      } // end else zdata
#else // GBASISF4_BUCHBERGER 
      if (!in_gbasismod(current,resmod,G,p.val,true,&reduceto0)){
	ok=false;
	break;
      }
      // CERR << "reduceto0 " << reduceto0.size() << '\n';
      //if (!in_gbasis(current,G,&env)) return false;
#endif // GBASISF4_BUCHBERGER 
#ifdef HAVE_LIBPTHREAD
      // finish threads before chinese remaindering
      void * threadretval[MAXNTHREADS];
      for (int t=0;t<th;++t){
	threadretval[t]=&threadretval; // non-0 initialization
	pthread_join(tab[t],&threadretval[t]);
      }
#endif
      // cleanup G
      for (unsigned i=0;i<G.size();++i){
	if (resmod[G[i]].coord.empty()){
	  G.erase(G.begin()+i);
	  --i;
	}
      }
      double t_1=CLOCK()*1e-6;
      if (time1strun<0){
	time1strun=t_1-t_0;
	prevreconpart=0.0; // (1.0+res.size())/G.size();
      }
      else {
	if (time2ndrun<0){
	  time2ndrun=(t_1-t_0)/(th+1); // we are computing th+1 primes
	  if (time1strun>=1 || debug_infolevel)
	    CERR << "// Timing for 2nd run " << time2ndrun << " 1st run " << time1strun << " speed ratio " << time2ndrun/time1strun << " [current reconstructed ratio for reinjection=" <<  gbasis_reinject_ratio << " speed_ratio for reinjection=" << gbasis_reinject_speed_ratio << " modifiable by gbasis_reinject(reconstructed_ratio,speed_ratio) command]" << '\n';
	  if (time2ndrun<time1strun*gbasis_reinject_speed_ratio 
	      || gbasis_par.reinject_for_calc>0
	      //|| time2ndrun<0.5
	      ){
	    // learning is fast enough, don't try reinjection
	    if (augmentgbasis>0)
	      augmentgbasis=2;
	  }
	} 
      }
      if (debug_infolevel){
	CERR << t_1 << " end, basis size " << G.size() << " prime number " << primecount+1 << '\n';
      }
      /* ***************************************************
       *  EXTRACT to gbmod, zlmmod, zlmmodradical and rurv
       *****************************************************  */
      unsigned i=0; // effth==th+1 or ==(th+1)*4
      for (int efft=0;efft<effth;++efft){
        ++primecount;
	int t=efft*sizeof(modint)/sizeof(qmodint_t);
	int ttab=efft % (sizeof(qmodint_t)/sizeof(modint));
	rurv.clear();
	if (t==th){
          pcur=getint(p_qmodint,ttab);
	  zrur=mainthrurinzgbasis;
	  if (zrur){
	    convert(mainthrurlm,zlmmod,ttab); // zlmmod=mainthrurlm;
	    convert(mainthrurlmmodradical,zlmmodradical,ttab); // zlmmodradical=mainthrurlmmodradical;
	    convert(mainthrurv,rurv,ttab);//rurv.swap(mainthrurv);
	  }
	  // extract from current
          if (coeffsmodptr){
            initgensize=gbasiscoeffv[th].front().size();
            gbmod.resize(G.size()*(1+initgensize));
            int pos=0;
            for (i=0;i<G.size();++i){
              convert(resmod[G[i]],gbmod[pos],ttab); // gbmod[pos]=resmod[G[i]];
              ++pos;
              for (int j=0;j<initgensize;++j){
                convert(gbasiscoeffv[th][G[i]][j],gbmod[pos],ttab); // gbmod[pos]=gbasiscoeffv[th][G[i]][j];
                ++pos;
              }
            }
#if 0
            if (0){
              ofstream l((string("log")+p.print()).c_str());
              for (int i=0;i<gbmod.size();++i){
                l << i/(1+initgensize) << "," << i%(1+initgensize) << ":" << gbmod[i] << "\n";
              }
              l.close();
              if (th) exit(0);
            }
#endif
          } // if (coeffsmodptr)
          else {
            if (rur || gbmod.size()<G.size())
              gbmod.resize(G.size());
            for (i=0;i<G.size();++i){
              convert(resmod[G[i]],gbmod[i],ttab); // gbmod[i]=resmod[G[i]];
            }
	  }
	  // CERR << "read " << t << " " << p << '\n';
	} // if (t==th) main thread
#ifdef HAVE_LIBPTHREAD
	else {
	  void * ptr_=(void *)threadretval[t]; // saved value from ptr_join
	  if (!ptr_)
	    continue;
	  thread_gbasis_t<tdeg_t,qmodint_t> * ptr = (thread_gbasis_t<tdeg_t,qmodint_t> *) ptr_;
	  // extract from current
	  zrur=ptr->rurinzgbasis;
	  if (zrur){
	    convert(ptr->rurlm,zlmmod,ttab); // zlmmod=ptr->rurlm;
	    convert(ptr->rurlmmodradical,zlmmodradical,ttab); // zlmmodradical=ptr->rurlmmodradical;
	    convert(ptr->rurv,rurv,ttab); // rurv.swap(ptr->rurv);
	  }
          if (coeffsmodptr){
            initgensize=gbasiscoeffv[th].front().size();
            gbmod.resize(ptr->G.size()*(1+initgensize));
            int pos=0;
            for (i=0;i<ptr->G.size();++i){
              convert(ptr->resmod[ptr->G[i]],gbmod[pos],ttab); // gbmod[pos]=ptr->resmod[ptr->G[i]];
              ++pos;
              for (int j=0;j<initgensize;++j){
                convert(gbasiscoeffv[t][ptr->G[i]][j],gbmod[pos],ttab); // gbmod[pos]=gbasiscoeffv[t][ptr->G[i]][j];
                ++pos;
              }
            }
#if 0
            if (0){
              ofstream l((string("log_")+print_INT_(gbasis_param[t].p)).c_str());
              for (int i=0;i<gbmod.size();++i)
                l << i/(1+initgensize) << "," << i%(1+initgensize) << ":" << gbmod[i] << "\n";
              l.close();
            }
#endif
          }
          else {
            if (rur || gbmod.size()<ptr->G.size())
              gbmod.resize(ptr->G.size());
            for (i=0;i<ptr->G.size();++i)
              convert(ptr->resmod[ptr->G[i]],gbmod[i],ttab); // gbmod[i]=ptr->resmod[ptr->G[i]];
          }
          pcur=getint(ptr->p,ttab);
	  // CERR << "read " << t << " " << p << '\n';
	  ++count;
	  ++recon_count;
	}
#endif
      /* *************************
       *  RECONSTRUCT
       ****************************  */
	if (!ok)
	  continue;
        if (!coeffsmodptr){
          // remove 0 from gbmod
          remove_zero(gbmod);
          // if augmentgbasis>0 (at least) gbmod must be sorted
          //if (augmentgbasis>0)
          sort(gbmod.begin(),gbmod.end(),tripolymod_tri<polymod<tdeg_t,modint> >(gbasis_logz_age));
        }
	rur_gblm(gbmod,cur_gblm);
	if (prev_gblm.coord.empty())
	  prev_gblm=cur_gblm;
	else {
	  int cmp=compare_gblm(cur_gblm,prev_gblm);
	  if (cmp==1){
	    if (debug_infolevel) CERR << "Unlucky prime " << pcur << "\n";
	    continue; // bad prime
	  }
	  if (cmp==-1){ // clear and restart!
	    recon_n1=-1;
	    prev_gblm.coord.clear();
	    gbasis_size=G.size();
	    f4buchberger_info.clear();
	    zf4buchberger_info.clear();
	    reduceto0.clear();
	    V.clear(); W.clear(); Wlast.clear(); P.clear(); Wrur.clear();
	  }
	}
	if (gbasis_size==-1 || gbasis_size<G.size())
	  gbasis_size=G.size();
	if (!rur && gbasis_stop<0)
	  gbmod.resize(-gbasis_stop);
	if (count==0 && V.empty() && gbasis_par.reinject_begin>=0 && gbasis_par.reinject_end>gbasis_par.reinject_begin){
	  // initial reinjection
	  int K=gbasis_par.reinject_end-gbasis_par.reinject_begin;
	  Wlast.push_back(vectpoly8<tdeg_t>()); 
	  reverse(toreinject.begin(),toreinject.end());
	  toreinject.resize(K);
	  Wlast[0].swap(toreinject);
	  for (int k=0;k<K;++k){
	    if (!chk_equal_mod(Wlast[0][k],gbmod[k],pcur)){
	      *logptr(contextptr) << CLOCK() << " reinjection failure at position " << k << '\n';
	      Wlast.clear(); 
	      break;
	    }
	  }
	  if (!Wlast.empty()){ // reinjection ok
	    *logptr(contextptr) << CLOCK() << " reinjection success " << K << '\n';
	    V.push_back(vectpoly8<tdeg_t>());
	    W.push_back(vectpoly8<tdeg_t>()); 
	    convert(gbmod,V.back(),pcur);
	    recon_added=gbasis_par.reinject_end-gbasis_par.reinject_begin;
	    prevreconpart=recon_added/double(gbmod.size());    
	    for (int k=0;k<K;++k){
	      V[0][k].coord.clear();
	    }
	    P.push_back(pcur);
	    continue; // next prime
	  }
	}
	nmonoms=0;
	for (size_t j=0;j<gbmod.size();++j){
	  if (debug_infolevel>1) CERR << j << "(" << gbmod[j].age << "," << gbmod[j].logz << ":" << gbmod[j].fromleft << "," << gbmod[j].fromright << ")" << '\n';
	  nmonoms += gbmod[j].coord.size();
	}
	if (rur_do_gbasis>0 && nmonoms>rur_do_gbasis)
	  rur_gbasis=false;
	if (debug_infolevel && count==0 && ttab==0){
	  CERR << "G= index_in_gbasis:index_computed(age,logz,fromleft,fromright)\n";
	  int maxlogz=0;
	  for (size_t i=0;i<G.size();++i){
	    maxlogz=giacmax(maxlogz,resmod[G[i]].logz);
	    CERR << i << ":" << G[i] << "(" << resmod[G[i]].age<<"," << resmod[G[i]].logz << ":" << resmod[G[i]].fromleft << "," << resmod[G[i]].fromright << ")" << '\n';
	  }
	  CERR << "maxlogz " << maxlogz << '\n';
	  CERR << '\n' << "Partial number of monoms " << nmonoms << '\n';
	}
	// compare gb to existing computed basis
#if 1
	unsigned jpos; gen num,den; 
	if (rur){
	  gbmod.resize(G.size());
	  dim=res.front().dim;
	  int rqi;
	  if (zrur){
	    rqi=zlmmod.coord.size();
	  }
	  else {
#ifdef GBASIS_4PRIMES
            polymod<tdeg_t,modint> mainthrurgblm_,mainthrurlmsave_;
            convert(mainthrurgblm,mainthrurgblm_,ttab);
            convert(mainthrurlmsave,mainthrurlmsave_,ttab);
	    rqi=rur_quotient_ideal_dimension<tdeg_t,modint>(gbmod,zlmmod,&mainthrurgblm_,&mainthrurlmsave_);
            // FIXME??
            // convert(mainthrurgblm_,mainthrurgblm);
            // convert(mainthrurlmsave_,mainthrurlmsave);
#else
	    rqi=rur_quotient_ideal_dimension(gbmod,zlmmod,&mainthrurgblm,&mainthrurlmsave);
#endif            
	  }
	  if (rqi==-RAND_MAX)
	    *logptr(contextptr) << "Overflow in rur, computing revlex gbasis\n";
	  if (rqi<0){
	    if (rur_error_ifnot0dimensional){
	      res.clear();
	      mpz_clear(zd);
	      mpz_clear(zu);
	      mpz_clear(zu1);
	      mpz_clear(zd1);
	      mpz_clear(zabsd1);
	      mpz_clear(zsqrtm);
	      mpz_clear(zq);
	      mpz_clear(zur);
	      mpz_clear(zr);
	      mpz_clear(ztmp);
	      return 1;
	    }
	    rur=0;
	    continue;
	  }
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " begin modular rur check" << '\n';
	  if (rur==2){
	    vecteur m,M,res; 
	    polymod<tdeg_t,modint> s(order,dim);
	    index_t l(dim);
	    l[dim-1]=1;
	    s.coord.push_back(T_unsigned<modint,tdeg_t>(1,tdeg_t(l,order)));
	    ok=rur_minpoly<tdeg_t,modint,modint2>(gbmod,zlmmod,s,pcur,m,M);
	    rur_convert_univariate(m,dim-1,gbmod[0]);
	    gbmod.resize(1);
	  }
	  else {
	    bool ok=true;
	    if (!zrur)
	      ok=rur_compute<tdeg_t>(gbmod,zlmmod,zlmmodradical,pcur,rurs,&gbasis_par.initsep,rurv);
	    if (!ok){
	      if (zlmmodradical.coord.empty()){ 
		CERR << CLOCK()*1e-6 << " Unable to compute modular rur\n";
		ok = false; rur = 0; 
	      }
	      else
		CERR << CLOCK()*1e-6 << " Bad prime, ignored\n";
	      continue;
	    }
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " end modular rur check" << '\n';
	    if (rur_gbasis){ // reconstruct gbasis and rur
	      for (int r=0;r<rurv.size();++r){
		gbmod.push_back(rurv[r]);
	      }
	    }
	    else
	      gbmod.swap(rurv); // reconstruct the rur instead of the gbasis
	  } // check for bad primes
	  if (zlmmodradical.coord.empty())
	    zlmmodradical=zlmmod;
	  else {
	    int i=rur_compare(zlmmodradical,zlmmod);
	    if (i!=0){
	      if (i==1) // zlmmodradical!=lmtmp and contains lmtmp, bad prime
		continue;
	      // clear existing reconstruction
	      recon_n1=-1;
	      prev_gblm.coord.clear();
	      gbasis_size=G.size();
	      f4buchberger_info.clear();
	      zf4buchberger_info.clear();
	      reduceto0.clear();
	      V.clear(); W.clear(); Wlast.clear(); P.clear(); Wrur.clear();
	      if (i==-1)
		continue;
	      // restart with this prime
	    }
	  }
	  // check Wrur
	  if (rur_gbasis){
	    for (jpos=0;jpos<Wrur.size();++jpos){
	      if (!chk_equal_mod(Wrur[jpos],gbmod[gbasis_size+jpos],pcur)){
		Wrur.resize(jpos);
		break;
	      }
	    }
	    if (jpos==dim+3 && rur_certify(res,Wrur,0,contextptr)){ 
	      swap(res,Wrur);
	      mpz_clear(zd);
	      mpz_clear(zu);
	      mpz_clear(zu1);
	      mpz_clear(zd1);
	      mpz_clear(zabsd1);
	      mpz_clear(zsqrtm);
	      mpz_clear(zq);
	      mpz_clear(zur);
	      mpz_clear(zr);
	      mpz_clear(ztmp);
	      if (debug_infolevel)
                *logptr(contextptr) << "#Primes " << count <<'\n';	    
	      return 1;
	    }	    
	  }
	} // end if (rur)
	if (debug_infolevel>2)
	  CERR << "p=" << pcur << ":" << gbmod << '\n';
	for (i=0;i<V.size();++i){
	  if (W.size()<V.size())
	    W.resize(V.size());
	  if (Wlast.size()<V.size()){
	    Wlast.resize(V.size());
	    Wlast.back().reserve(gbmod.size());
	  }
	  if (V[i].size()!=gbmod.size())
	    continue;
	  for (jpos=recon_added;jpos<gbmod.size();++jpos){
	    if (V[i][jpos].coord.empty() && gbmod[jpos].coord.empty())
	      continue;
	    if (V[i][jpos].coord.empty())
	      break;
	    if (gbmod[jpos].coord.empty())
	      break;
	    if (V[i][jpos].coord.front().u!=gbmod[jpos].coord.front().u)
	      break;
	  }
	  if (jpos!=gbmod.size()){
	    rechecked=0;
	    continue;
	  }
 	  if (rur>=0 && eps>1e-20)
	    jpos_start=giacmax(0,giacmin(recon_n0,giacmin(recon_n1,recon_n2)));
	  else
	    jpos_start=recon_added; // 0 or recon_added (do not check already reconstructed)
	  jpos=jpos_start;
	  // check existing Wlast 
	  for (;jpos<Wlast[i].size();++jpos){
	    if (!chk_equal_mod(Wlast[i][jpos],gbmod[jpos],pcur)){
	      Wlast[i].resize(jpos);
	      rechecked=0;
	      break;
	    }
	  }
	  if (jpos!=Wlast[i].size() || P[i].type==_INT_){
	    // CERR << jpos << '\n';
	    // IMPROVE: make it work for rur!
	    if (!rur && eps>0 && P[i].type==_INT_ && recon_added==0 && gbasis_stop!=0){
	      // check for non modular gb with early reconstruction */
	      // first build a candidate in early with V[i]
	      vectpoly8<tdeg_t> early(V[i]);
	      int d;
	      for (jpos=0;jpos<early.size();++jpos){
		d=1;
		if (!findmultmod(early[jpos],P[i].val,d)){
		  if (debug_infolevel>1)
		    COUT << "early reconstr. failure pos " << jpos << " P=" << early[jpos] << " d=" << d << " modulo " << P[i].val << '\n';
		  break;
		}
		int s=int(early[jpos].coord.size());
		for (int k=0;k<s;++k){
		  early[jpos].coord[k].g=smod(extend(early[jpos].coord[k].g.val)*d,P[i].val);
		}
	      }
	      // then check
	      if (jpos==early.size()){
		for (jpos=0;jpos<early.size();++jpos){
		  polymod<tdeg_t,modint> tmp(gbmod[jpos]);
		  smallmultmod(early[jpos].coord.front().g.val,tmp,pcur);
		  if (!chk_equal_mod(early[jpos],tmp,pcur)){
		    if (debug_infolevel>1)
		      COUT << "early recons. failure jpos=" << jpos << " " << early[jpos] << " " << tmp << " modulo " << pcur << '\n';
		    break;
		  }
		}
		rechecked=0; 
		if (jpos==early.size() && (eliminate_flag || check_initial_generators(res,early,G,eps))){
		  if (debug_infolevel)
		    CERR << CLOCK()*1e-6 << " end final check " << '\n';
		  swap(res,early);
		  mpz_clear(zd);
		  mpz_clear(zu);
		  mpz_clear(zu1);
		  mpz_clear(zd1);
		  mpz_clear(zabsd1);
		  mpz_clear(zsqrtm);
		  mpz_clear(zq);
		  mpz_clear(zur);
		  mpz_clear(zr);
		  mpz_clear(ztmp);
                  if (debug_infolevel)
                    *logptr(contextptr) << "#Primes " << count <<'\n';	    
		  return 1;
		}
	      } // end jpos==early.size()
	    } // end if !rur ...
	    break; // find another prime
	  }
	  if (debug_infolevel && (rur || t==th))
            CERR << CLOCK()*1e-6 << " checking\n";
	  for (;jpos<V[i].size();++jpos){
	    unsigned Vijs=unsigned(V[i][jpos].coord.size());
	    if (Vijs!=gbmod[jpos].coord.size()){
	      rechecked=0;
	      if (debug_infolevel>1)
		CERR << "chinrem size mismatch " << jpos << '\n';
	      break;
	    }
	    //Vijs=1; 
	    bool dobrk=false;
	    int chks[]={int(.1*Vijs),int(Vijs/2), int(.9*Vijs)};
	    //int chks[]={Vijs/2, int(.9*Vijs)};
	    for (int chk=0;chk<sizeof(chks)/sizeof(int);++chk){
              if (!rur && (t<th)){
                dobrk=true;
                break;
              }
	      Vijs=chks[chk];
	      if (Vijs && V[i][jpos].coord[Vijs].g.type==_ZINT){
		if (!in_fracmod(P[i],V[i][jpos].coord[Vijs].g,
				zd,zd1,zabsd1,zu,zu1,zur,zq,zr,zsqrtm,ztmp,num,den)){
		  rechecked=0;
		  if (debug_infolevel>1)
		    CERR << jpos << '\n';
		  dobrk=true;
		  break;
		}
		modint gg=gbmod[jpos].coord[Vijs].g;
		if (!chk_equal_mod(num/den,gg,pcur)){
		  rechecked=0;
		  if (debug_infolevel>1)
		    CERR << jpos << '\n';
		  dobrk=true;
		  break;
		}
	      }
	    }
	    if (dobrk){
	      if (rur_gbasis && rur>0 && jpos<gbasis_size){ // go try to reconstruct the rur part
		jpos=gbasis_size-1; continue;
	      }
	      break;
	    }
            int chkfrac=fracmod(V[i][jpos],P[i],
                                zd,zd1,zabsd1,zu,zu1,zur,zq,zr,zsqrtm,ztmp,
                                poly8tmp,&gbmod[jpos],pcur);
	    if (chkfrac==0){
	      rechecked=0;
	      CERR << CLOCK()*1e-6 << " reconstruction failure at position " << jpos << '\n';
	      break;
	    }
	    if (rur && !poly8tmp.coord.empty() && !chk_equal_mod(poly8tmp.coord.front().g,gbmod[jpos].coord.front().g,pcur)){
	      rechecked=0;
	      if (rur_gbasis && rur>0 && jpos<gbasis_size){ // go try to reconstruct the rur part
		jpos=gbasis_size-1; continue;
	      }
	      break;
	    }
	    if (chkfrac==2 || !chk_equal_mod(poly8tmp,gbmod[jpos],pcur)){
              if (chkfrac==2 && debug_infolevel)
                CERR << CLOCK()*1e-6 << " unstable modular check at position " << jpos << "\n";
	      rechecked=0;
	      if (rur_gbasis && rur>0 && jpos<gbasis_size){ // go try to reconstruct the rur part
		jpos=gbasis_size-1; continue;
	      }
	      break;
	    }
	    poly8<tdeg_t> tmptmp(poly8tmp.order,poly8tmp.dim);
	    if (rur_gbasis && rur>0 && jpos>=gbasis_size){
	      if (jpos>=gbasis_size+Wrur.size()){
		Wrur.push_back(tmptmp);
		Wrur.back().coord.swap(poly8tmp.coord);
	      }
	    }
	    else {
	      Wlast[i].push_back(tmptmp);
	      Wlast[i].back().coord.swap(poly8tmp.coord);
	    }
	  }
	  if (debug_infolevel>0){
	    CERR << CLOCK()*1e-6 << " unstable mod " << pcur << " from " << gbasis_size ;
            if (coeffsmodptr)
              CERR << "*(1+" << initgensize << ")";
            CERR << " reconstructed " << Wlast[i].size() << " (#" << i << ")" << '\n';
          }
	  // possible improvement: if t==th and i==0 and Wlast.size()/V[i].size() 
	  // has increased significantly
	  // it might be a good idea to add it's component 
	  // to current, and clear info (if zdata: reduceto0, zf4buchberger_info)
	  recon_n2=recon_n1;
	  recon_n1=recon_n0;
	  recon_n0=Wlast[i].size();
	  if (eps>1e-20 && !gbasis_par.gbasis &&
	      ttab==sizeof(qmodint_t)/sizeof(modint)-1 && // check only for the last prime of parallel threads
	      // recon_n2==recon_n1 && recon_n1==recon_n0 &&
	      zdata && augmentgbasis && t==th && i==0){
	    if (rur_gbasis && rur==1 && recon_n2>=gbasis_size){ // the gbasis is known
	      rur=-gbasis_size; recon_added=gbasis_size;
	      current_gbasis=Wlast[i];
	      current_gbasis.erase(current_gbasis.begin()+gbasis_size,current_gbasis.end());
	      cleardeno(current_gbasis);
	    }
	    double reconpart=recon_n2/double(V[i].size());
	    if (!rur && !coeffsmodptr &&
		recon_n0/double(V[i].size())<0.95 && 
		(reconpart-prevreconpart>augmentgbasis 
		 // || (reconpart>prevreconpart && recon_count>=giacmax(128,th*4))
		 )
		){
              double tt=CLOCK()*1e-6;
              if (tt>2 || debug_infolevel)
                CERR << "// " << tt << " adding reconstructed ideal generators " << recon_n2 << " (reconpart " << reconpart << " prev " << prevreconpart << " augment " << augmentgbasis << " recon_count " << recon_count << " th " << th << " recon_n2 " << recon_n2 << " V[i] " << V[i].size() << ")" << '\n';
	      recon_count=0;
	      prevreconpart=reconpart;
	      if (rur && recon_added>gbasis_size)
		recon_added=gbasis_size;
	      //current_gbasis=current_orig;
	      int insertpos=0;
	      for (int k=recon_added;k<recon_n2;++k){
		V[i][k].coord.clear();
		poly8<tdeg_t> tmp=Wlast[i][k];
		cleardeno(tmp);
		for (;insertpos<current_gbasis.size();++insertpos){
		  if (tdeg_t_greater(current_gbasis[insertpos].coord.front().u,tmp.coord.front().u,order)){
		    if (!(current_gbasis[insertpos]==tmp)){
		      current_gbasis.insert(current_gbasis.begin()+insertpos,tmp);
		      ++insertpos;
		    }
		    break;
		  }		    
		}
		if (insertpos==current_gbasis.size()){
		  current_gbasis.push_back(tmp);
		  ++insertpos;
		}
	      }
	      recon_added=recon_n2; // Wlast[i].size();
              tt=CLOCK()*1e-6;
              if (tt>2 || debug_infolevel)
                CERR << "// " << tt << " # new ideal generators " << current_gbasis.size() << '\n';
	      reduceto0.clear();
	      zf4buchberger_info.clear();
	      if (gbasis_logz_age){
		res.swap(current_gbasis);
		mpz_clear(zd);
		mpz_clear(zu);
		mpz_clear(zu1);
		mpz_clear(zd1);
		mpz_clear(zabsd1);
		mpz_clear(zsqrtm);
		mpz_clear(zq);
		mpz_clear(zur);
		mpz_clear(zr);
		mpz_clear(ztmp);
		return -1;
	      }
	    }
	  }
	  break;
	} // end for loop on i
	if (i==V.size()){
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " creating reconstruction #" << i << '\n';
	  // not found
	  V.push_back(vectpoly8<tdeg_t>());
	  convert(gbmod,V.back(),pcur);
	  W.push_back(vectpoly8<tdeg_t>()); // no reconstruction yet, wait at least another prime
	  Wlast.push_back(vectpoly8<tdeg_t>());
	  P.push_back(pcur);
	  continue; // next prime
	}
	if (!rur && 
	    gbasis_stop<0 && recon_n0>=-gbasis_stop){
	  if (recon_n2<-gbasis_stop)
	    continue;
	  // stop here
	  W[i]=Wlast[i];
	  W[i].resize(recon_n2);
	  cleardeno(W[i]); // clear denominators
	  CERR << CLOCK()*1e-6 << " Max number of generators reconstructed " << jpos << ">=" << -gbasis_stop << '\n';
	  swap(res,W[i]);
	  mpz_clear(zd);
	  mpz_clear(zu);
	  mpz_clear(zu1);
	  mpz_clear(zd1);
	  mpz_clear(zabsd1);
	  mpz_clear(zsqrtm);
	  mpz_clear(zq);
	  mpz_clear(zur);
	  mpz_clear(zr);
	  mpz_clear(ztmp);
          if (debug_infolevel)
            *logptr(contextptr) << "#Primes " << count <<'\n';	    
	  return 1;
	}
	if (jpos<gbmod.size()){
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " i=" << i << " begin chinese remaindering " << pcur << " (" << primecount << ")" << '\n';
	  int r=chinrem(V[i],P[i],gbmod,pcur,poly8tmp,recon_added,nthreads);// was jpos_start); but fails for cyclic7 // IMPROVE: maybe start at jpos in V[i]? at least start at recon_added
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " end chinese remaindering" << '\n';
	  if (r==-1){
	    ok=false;
	    continue;
	  }
	  P[i]=pcur*P[i];
	  continue; // next prime
	}
	else if (Wrur.size()<dim+4) { // was dim+2, but dim+4 required if gbasis_par.gbasis is true
          // final check
	  W[i]=Wlast[i];
	  if (!rur && !coeffsmodptr){
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " stable, clearing denominators " << '\n';
            cleardeno(W[i]); // clear denominators
	  }
	  ++rechecked;
          double tend=CLOCK()*1e-6 ;
	  if (debug_infolevel || tend-t_0>5)
	    *logptr(contextptr) << "// Groebner basis computation time=" << clock_realtime()-init_time << " memory " << memory_usage()*1e-6 << "M" << (chk_initial_generator?": end rational reconstruction ":": end additional prime check") << '\n';
	  efft=effth; // avoid unlucky prime messages
	  // now check if W[i] is a Groebner basis over Q, if so it's the answer
	  if (rur && rur!=2 && !gbasis_par.gbasis && rur_certify(res,W[i],rur_gbasis?gbasis_size:0,contextptr)){ // rur!=2 was added otherwise crash for eliminate([-v5+1,-v6,v7-1,v8-1,v10^2-v6^2-v5^2+2*v6-1,v9^2-1,v9*m-v10,v9*n-1],[v1,v2,v3,v4,v5,v6,v7,v8,v10,v9,n])
	    swap(res,W[i]);
	    if (rur_gbasis)
	      res.erase(res.begin(),res.begin()+gbasis_size);
            goto cleanup;
          }
	  if (rur && rur!=2 && gbasis_par.gbasis && rur_certify(res,rur_gbasis?Wrur:W[i],0,contextptr)){ // rur!=2 was added otherwise crash for eliminate([-v5+1,-v6,v7-1,v8-1,v10^2-v6^2-v5^2+2*v6-1,v9^2-1,v9*m-v10,v9*n-1],[v1,v2,v3,v4,v5,v6,v7,v8,v10,v9,n])
	    if (rur_gbasis){
	      swap(res,Wrur);
              for (int k=0;k<W[i].size();++k)
                res.push_back(W[i][k]);
            }
            else
              swap(res,W[i]);
          cleanup:
	    mpz_clear(zd);
	    mpz_clear(zu);
	    mpz_clear(zu1);
	    mpz_clear(zd1);
	    mpz_clear(zabsd1);
	    mpz_clear(zsqrtm);
	    mpz_clear(zq);
	    mpz_clear(zur);
	    mpz_clear(zr);
	    mpz_clear(ztmp);
            if (debug_infolevel)
              *logptr(contextptr) << "#Primes " << count <<'\n';	    
	    return 1;
	  }
          if (coeffsmodptr){
	    vectpoly8<tdeg_t> & cur=W[i];
            res.clear();
            int pos=0;
            coeffsmodptr->resize(G.size());
            for (int i=0;i<G.size();++i){
              res.push_back(cur[pos]);
              ++pos;
              (*coeffsmodptr)[i].resize(initgensize);
              for (int j=0;j<initgensize;++j){
                (*coeffsmodptr)[i][j]=cur[pos];
                ++pos;
              }
            }
            goto cleanup;
          }
	  // first verify that the initial generators reduce to 0
	  if (!eliminate_flag && chk_initial_generator && !check_initial_generators(res,W[i],G,eps))
	    continue;
	  if (int(W[i].size())<=GBASIS_DETERMINISTIC)
	    eps=0;
	  if (eliminate_flag && eps==0)
	    eps=1e-7;
	  double eps2=std::pow(double(pcur),double(rechecked))*eps;
	  // recheck by computing gbasis modulo another prime
	  if (eps2>0 && eps2<1){
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " Final check successful, running another prime to increase confidence." << '\n';
            chk_initial_generator=false;
	    continue;
	  }
	  if (eps>0){
	    double terms=0;
	    int termsmin=RAND_MAX; // estimate of the number of terms of a reduced non-0 spoly
	    for (unsigned k=0;k<W[i].size();++k){
	      terms += W[i][k].coord.size();
	      termsmin = giacmin(termsmin,unsigned(W[i][k].coord.size()));
	    }
	    termsmin = 7*(2*termsmin-1);
	    int epsp=P[i].type==_ZINT?mpz_sizeinbase(*P[i]._ZINTptr,10):8-int(std::ceil(2*std::log10(terms)));
	    if (epsp>termsmin)
	      epsp=termsmin;
            *logptr(contextptr) << "// Non determinisitic Groebner basis algorithm over the rationals. " <<
              (eps<1.01e-10?gettext("Reconstructed Groebner basis checked with an additional prime. If successful, error"):"Error")
               << " probability is less than " << eps << gettext(" and is estimated to be less than 10^-") << epsp << gettext(". Use proba_epsilon:=0 to certify (this takes more time).") << '\n';
	  }
	  G.clear();
	  if (eps<1.01e-10){
	    // check modulo another prime that W[i] is a gbasis
	    vector<unsigned> G;
	    vectpoly8<tdeg_t> res_(W[i]);
	    vectpolymod<tdeg_t,modint> resmod;
	    vector< zinfo_t<tdeg_t> > zf4buchberger_info;
	    int p=268435399;
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " Checking that the basis is a gbasis modulo " << p << '\n';
	    if (!zgbasis<tdeg_t,modint,modint2>(res_,resmod,G,p,true,0,zf4buchberger_info,false,false,false,false,threads /* parallel*/,true,&gbasis_par.initsep,gbasis_par,0))
	      return 0;
	    sort(resmod.begin(),resmod.end(),tripolymod_tri<polymod<tdeg_t,modint> >(false));
	    sort(W[i].begin(),W[i].end(),tripolymod_tri<poly8<tdeg_t> >(false));
	    for (size_t jpos=0;jpos<G.size();++jpos){
	      if (!chk_equal_mod(W[i][jpos],resmod[jpos],p))
		return 0;
	    }
	    if (debug_infolevel)
	      CERR << CLOCK()*1e-6 << " Check successful mod " << p << '\n';
	  }
	  if (eps2<1 && !is_gbasis(W[i],eps2,modularcheck)){
	    ok=false;
	    continue; // in_gbasis(W[i],G,0,true);
	  }
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " end final check " << '\n';
	  swap(res,W[i]);
	  mpz_clear(zd);
	  mpz_clear(zu);
	  mpz_clear(zu1);
	  mpz_clear(zd1);
	  mpz_clear(zabsd1);
	  mpz_clear(zsqrtm);
	  mpz_clear(zq);
	  mpz_clear(zur);
	  mpz_clear(zr);
	  mpz_clear(ztmp);
	  return 1;
	}
#else
	for (i=0;i<V.size();++i){
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " i= " << i << " begin chinese remaindering" << '\n';
	  int r=chinrem(V[i],P[i],gbmod,pcur,poly8tmp);
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " end chinese remaindering" << '\n';
	  if (r==-1){
	    ok=false;
	    break;
	  }
	  if (r==0){
	    CERR << CLOCK()*1e-6 << " leading terms do not match with reconstruction " << i << " modulo " << p << '\n';
	    continue;
	  }
	  // found one! V is already updated, update W
	  if (W.size()<V.size())
	    W.resize(V.size());
	  if (Wlast.size()<V.size())
	    Wlast.resize(V.size());
	  P[i]=P[i]*p;
	  unsigned jpos=0;
	  // afewpolys.clear();
	  for (;jpos<V[i].size();++jpos){
	    if (int(Wlast[i].size())>jpos && chk_equal_mod(Wlast[i][jpos],gb[jpos],pcur)){
	      if (afewpolys.size()<=jpos)
		afewpolys.push_back(Wlast[i][jpos]);
	      else {
		if (!(afewpolys[jpos]==Wlast[i][jpos]))
		  afewpolys[jpos]=Wlast[i][jpos];
	      }
	    }
	    else {
	      if (!fracmod(V[i][jpos],P[i],
			   zd,zd1,zabsd1,zu,zu1,zur,zq,zr,zsqrtm,ztmp,
			   poly8tmp)){
		CERR << CLOCK()*1e-6 << " reconstruction failure at position " << jpos << '\n';
		break;
	      }
	      if (afewpolys.size()<=jpos){
		poly8<tdeg_t> tmp(poly8tmp.order,poly8tmp.dim);
		afewpolys.push_back(tmp);
	      }
	      afewpolys[jpos].coord.swap(poly8tmp.coord);
	    }
	    if (Wlast[i].size()>jpos && !(afewpolys[jpos]==Wlast[i][jpos])){
	      if (debug_infolevel){
		unsigned j=0,js=giacmin(afewpolys[jpos].coord.size(),Wlast[i][jpos].coord.size());
		for (;j<js;++j){
		  if (!(afewpolys[jpos].coord[j]==Wlast[i][jpos].coord[j]))
		    break;
		}
		CERR << "Diagnostic: chinrem reconstruction mismatch at positions " << jpos << "," << j << '\n';
		if (j<js)
		  CERR << gb[jpos].coord[j].g << "*" << gb[jpos].coord[j].u << '\n';
		else
		  CERR << afewpolys[jpos].coord.size() << "," << Wlast[i][jpos].coord.size() << '\n';
	      }
	      afewpolys.resize(jpos+1);
	      break;
	    }
	    if (jpos > Wlast[i].size()*1.35+2 )
	      break;
	  }
	  if (afewpolys!=Wlast[i]){
	    swap(afewpolys,Wlast[i]);
	    if (debug_infolevel>0)
	      CERR << CLOCK()*1e-6 << " unstable mod " << p << " from " << V[i].size() << " reconstructed " << Wlast[i].size() << '\n';
	    break;
	  }
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " stable, clearing denominators " << '\n';
	  W[i]=Wlast[i];
	  cleardeno(W[i]); // clear denominators
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " end rational reconstruction " << '\n';
	  // now check if W[i] is a Groebner basis over Q, if so it's the answer
	  // first verify that the initial generators reduce to 0
	  poly8<tdeg_t> tmp0,tmp1,tmp2;
	  vectpoly8<tdeg_t> wtmp;
	  unsigned j=0,finalchecks=initial;
	  if (eps>0)
	    finalchecks=giacmin(2*W[i].front().dim,initial);
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " begin final check, checking that " << finalchecks << " initial generators belongs to the ideal" << '\n';
	  G.resize(W[i].size());
	  for (j=0;j<W[i].size();++j)
	    G[j]=j;
	  for (j=0;j<finalchecks;++j){
	    if (debug_infolevel)
	      CERR << "+";
	    reduce(res[j],W[i],G,-1,wtmp,tmp0,tmp1,tmp2,0);
	    if (!tmp0.coord.empty()){
	      break;
	    }
	    if (debug_infolevel	&& (j%10==9))
	      CERR << j+1 << '\n';
	  }
	  if (j!=finalchecks){
	    if (debug_infolevel){
	      CERR << CLOCK()*1e-6 << " final check failure, retrying with another prime " << '\n';
	      CERR << "Non-zero remainder " << tmp0 << '\n';
	      CERR << "checking res[j], " << j << "<" << initial << '\n';
	      CERR << "res[j]=" << res[j] << '\n';
	      CERR << "basis candidate " << W[i] << '\n';
	    }
	    break;
	}
	  /* (final check requires that we have reconstructed a Groebner basis,
	     Modular algorithms for computing Groebner bases Elizabeth A. Arnold
	     Journal of Symbolic Computation 35 (2003) 403–419)
	  */
#if 1
	  if (int(W[i].size())<=GBASIS_DETERMINISTIC)
	    eps=0;
	  if (eps>0){
	    double terms=0;
	    int termsmin=RAND_MAX; // estimate of the number of terms of a reduced non-0 spoly
	    for (unsigned k=0;k<W[i].size();++k){
	      terms += W[i][k].coord.size();
	      termsmin = giacmin(termsmin,W[i][k].coord.size());
	    }
	    termsmin = 7*(2*termsmin-1);
	    int epsp=mpz_sizeinbase(*P[i]._ZINTptr,10)-int(std::ceil(2*std::log10(terms)));
	    if (epsp>termsmin)
	      epsp=termsmin;
	    *logptr(contextptr) << gettext("Running a probabilistic check for the reconstructed Groebner basis. If successful, error probability is less than ") << eps << gettext(" and is estimated to be less than 10^-") << epsp << gettext(". Use proba_epsilon:=0 to certify (this takes more time).") << '\n';
	  }
	  G.clear();
	  if (eps<6e-8 && !is_gbasis(W[i],eps*1.677e7,modularcheck)){
	    ok=false;
	    break; // in_gbasis(W[i],G,0,true);
	  }
#endif
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " end final check " << '\n';
	  swap(res,W[i]);
	  mpz_clear(zd);
	  mpz_clear(zu);
	  mpz_clear(zu1);
	  mpz_clear(zd1);
	  mpz_clear(zabsd1);
	  mpz_clear(zsqrtm);
	  mpz_clear(zq);
	  mpz_clear(zur);
	  mpz_clear(zr);
	  mpz_clear(ztmp);
	  return 1;
	} // end for (i<V.size())
	if (i==V.size()){
	  if (debug_infolevel)
	    CERR << CLOCK()*1e-6 << " creating reconstruction #" << i << '\n';
	  // not found
	  V.push_back(gb);
	  W.push_back(vectpoly8<tdeg_t>()); // no reconstruction yet, wait at least another prime
	  Wlast.push_back(vectpoly8<tdeg_t>());
	  P.push_back(p);
	}
#endif
      } // end loop on threads
    } //end for int count
    mpz_clear(zd);
    mpz_clear(zu);
    mpz_clear(zu1);
    mpz_clear(zd1);
    mpz_clear(zabsd1);
    mpz_clear(zsqrtm);
    mpz_clear(zq);
    mpz_clear(zur);
    mpz_clear(zr);
    mpz_clear(ztmp);
    return 0;
  }
  
  template<class tdeg_t,class qmodint_t,class qmodint_t2>
  bool mod_gbasis(vectpoly8<tdeg_t> & res,bool modularcheck,bool zdata,int & rur,GIAC_CONTEXT,gbasis_param_t gbasis_param,vector< vectpoly8<tdeg_t> > * coeffsmodptr=0){
    int gbasis_logz_age=gbasis_logz_age_sort;
    for (;;){
      int tmp=in_mod_gbasis<tdeg_t,qmodint_t,qmodint_t2>(res,modularcheck,zdata,rur,contextptr,gbasis_param,gbasis_logz_age,coeffsmodptr);
#if 0 // def GIAC_4PRIMES
      // retry on error if zdata was enabled, maybe compressed monomials failed
      // FIXME use code -2 instead of 0
      if (zdata && tmp==0 && sizeof(qmodint_t)!=sizeof(modint))
        tmp=in_mod_gbasis<tdeg_t,modint,modint2>(res,modularcheck,false /* zdata*/,rur,contextptr,gbasis_param,gbasis_logz_age,coeffsmodptr);
#endif
      if (tmp!=-1) // -1 means part of the gbasis has been reconstructed
	return tmp;
      if (gbasis_logz_age)
	gbasis_logz_age=0; // special sorting is not meaningfull after 1 reinjection, and we want to have interreduction
    }
  }
#ifndef BIGENDIAN
#define GBASIS_SWAP 
#endif

#if !defined NO_STDEXCEPT && !defined BIGENDIAN
  #define GIAC_TDEG_T14
#endif

  // other tdeg_t types
#ifdef GIAC_TDEG_T14
#undef INT128 // it's slower!
  struct tdeg_t14 {
    bool vars64() const { return false;}
    int hash_index(void * ptr_) const {
      // if (!ptr_)
	return -1;
    }
    bool add_to_hash(void *ptr_,int no) const {
      return false;
    }
    void dbgprint() const;
    // data
    union {
      unsigned char tab[16]; // tab[0] and 1 is for total degree
      struct {
	unsigned char tdeg; 
	unsigned char tdeg2;
	order_t order_;
	longlong * ui;
      };
    };
    int front(){ return tab[2];}
    // methods
    inline unsigned selection_degree(order_t order) const {
#ifdef GBASIS_SELECT_TOTAL_DEGREE
      return total_degree(order);
#endif
      return tdeg;
    }
    inline unsigned total_degree(order_t order) const {
      return tdeg+tdeg2;
    }
    // void set_total_degree(unsigned d) { tab[0]=d;}
    tdeg_t14() { 
      longlong * ptr = (longlong *) tab;
      ptr[1]=ptr[0]=0;
    }
    tdeg_t14(int i){
      longlong * ptr = (longlong *) tab;
      ptr[1]=ptr[0]=0;
    }
    void get_tab(short * ptr,order_t order) const {
#ifdef GBASIS_SWAP
      tdeg_t14 t(*this);
      swap_indices14(t.tab);
#else
      const tdeg_t14 & t=*this;
#endif
      ptr[0]=t.tab[0];
      for (unsigned i=1;i<15;++i)
	ptr[i]=t.tab[i+1];
    }
    tdeg_t14(const index_m & lm,order_t order){ 
      longlong * ptr_ = (longlong *) tab;
      ptr_[1]=ptr_[0]=0;
      unsigned char * ptr=tab;
      vector<deg_t>::const_iterator it=lm.begin(),itend=lm.end();
      if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
	unsigned td=sum_degree(lm);
	if (td>=128) 
	  gensizeerr("Degree too large");
	*ptr=td;
	++ptr;
	*ptr=0;
	++ptr;
      }
      if (order.o==_REVLEX_ORDER){
	for (--itend,--it;it!=itend;++ptr,--itend)
	  *ptr=*itend;
      }
      else {
	for (;it!=itend;++ptr,++it)
	  *ptr=*it;
      }
#ifdef GBASIS_SWAP
      swap_indices14(tab);
#endif
    }
  };


#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const tdeg_t14 & x){
    os << "[";
    for (unsigned i=0; i<14;++i){
      os << unsigned(x.tab[i+2]) << ",";
    }
    return os << "]";
  }
#else
  ostream & operator << (ostream & os,const tdeg_t14 & x){
    os << "[";
    for (unsigned i=0; i<14;++i){
      os << unsigned(x.tab[i+2]) << ",";
    }
    return os << "]";
  }
#endif
  void tdeg_t14::dbgprint() const { COUT << * this << '\n'; }
  inline tdeg_t14 & operator += (tdeg_t14 & x,const tdeg_t14 & y){
#ifdef INT128
    * (uint128_t *) &x += * (const uint128_t *) &y;
#else
    // ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    *((ulonglong *)&x) += *((ulonglong *)&y);
    ((ulonglong *)&x)[1] += ((ulonglong *)&y)[1];
#endif
    if (x.tab[0]>=128){
      gensizeerr("Degree too large");
    }
    return x;  
  }
  inline tdeg_t14 operator + (const tdeg_t14 & x,const tdeg_t14 & y){
    tdeg_t14 res(x);
    return res += y;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]+ytab[0];
    ztab[1]=xtab[1]+ytab[1];
    return res;
  }
  inline void add(const tdeg_t14 & x,const tdeg_t14 & y,tdeg_t14 & res,int dim){
#ifdef INT128
    * (uint128_t *) &res = * (const uint128_t *) &x + * (const uint128_t *) &y;
#else
    // ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    *((ulonglong *)&res)=*((ulonglong *)&x)+*((ulonglong *)&y);
    ((ulonglong *)&res)[1]=((ulonglong *)&x)[1]+((ulonglong *)&y)[1];
#endif
    if (res.tab[0]>=128)
      gensizeerr("Degree too large");
  }
  tdeg_t14 operator - (const tdeg_t14 & x,const tdeg_t14 & y){ 
    tdeg_t14 res;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]-ytab[0];
    ztab[1]=xtab[1]-ytab[1];
    return res;
  }
  inline bool operator == (const tdeg_t14 & x,const tdeg_t14 & y){ 
#ifdef INT128
    return * (const uint128_t *) &x == * (uint128_t *) &y;
#else
    return ((longlong *) x.tab)[0] == ((longlong *) y.tab)[0] && ((longlong *) x.tab)[1] == ((longlong *) y.tab)[1];
#endif
  }
  inline bool operator != (const tdeg_t14 & x,const tdeg_t14 & y){ 
    return !(x==y);
  }

  static inline int tdeg_t14_revlex_greater (const tdeg_t14 & x,const tdeg_t14 & y){
#ifdef GBASIS_SWAP
#if 0
    longlong *xtab=(longlong *)&x,*ytab=(longlong *)&y;
    if (longlong a=*xtab-*ytab) // tdeg test already donne by caller
      return a<=0?1:0;
    if (longlong a=xtab[1]-ytab[1])
      return a<=0?1:0;
#else
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    if (xtab[0]!=ytab[0]) // tdeg test already donne by caller
      return xtab[0]<=ytab[0]?1:0;
    if (xtab[1]!=ytab[1])
      return xtab[1]<=ytab[1]?1:0;
#endif
    return 2;
#else // GBASIS_SWAP
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>=y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      if (x.tab[3]!=y.tab[3])
	return x.tab[3]<=y.tab[3]?1:0;
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]>=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      if (x.tab[11]!=y.tab[11])
	return x.tab[11]<=y.tab[11]?1:0;
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]<=y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
    }
    return 2;
#endif // GBASIS_SWAP
  }

  int tdeg_t14_lex_greater (const tdeg_t14 & x,const tdeg_t14 & y){
#ifdef GBASIS_SWAP
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    ulonglong X=*xtab, Y=*ytab;
    if (X!=Y){
      if ( (X & 0xffff) != (Y &0xffff))
	return (X&0xffff)>=(Y&0xffff)?1:0;
      return X>=Y?1:0;
    }
    if (xtab[1]!=ytab[1])
      return xtab[1]>=ytab[1]?1:0;
    return 2;
#else
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]>y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]>y.tab[2]?1:0;
      if (x.tab[3]!=y.tab[3])
	return x.tab[2]>y.tab[2]?1:0;
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]>y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]>y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]>y.tab[6]?1:0;
      return x.tab[7]>y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]>y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]>y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]>y.tab[10]?1:0;
      if (x.tab[11]!=y.tab[11])
	return x.tab[11]>y.tab[11]?1:0;
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]>y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]>y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]>y.tab[14]?1:0;
      return x.tab[15]>y.tab[15]?1:0;
    }
    return 2;
#endif
  }

  inline int tdeg_t_greater(const tdeg_t14 & x,const tdeg_t14 & y,order_t order){
    short X=x.tab[0];
    if (//order.o!=_PLEX_ORDER &&
        X!=y.tab[0]) return X>y.tab[0]?1:0; // since tdeg is tab[0] for plex
    if (order.o==_REVLEX_ORDER)
      return tdeg_t14_revlex_greater(x,y);
    return tdeg_t14_lex_greater(x,y);
  }
  inline bool tdeg_t_strictly_greater (const tdeg_t14 & x,const tdeg_t14 & y,order_t order){
    return !tdeg_t_greater(y,x,order); // total order
  }

#ifdef INT128
  uint128_t mask4=(((uint128_t) mask2)<<64)|mask2;
#endif
  
  inline bool tdeg_t_all_greater(const tdeg_t14 & x,const tdeg_t14 & y,order_t order){
#ifdef INT128
    return !((* (const uint128_t *) &x - * (const uint128_t *) &y) & mask4);
#else
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    if ((xtab[0]-ytab[0]) & mask2)
      return false;
    if ((xtab[1]-ytab[1]) & mask2)
      return false;
    return true;
#endif
  }

  // 1 (all greater), 0 (unknown), -1 (all smaller)
  int tdeg_t_compare_all(const tdeg_t14 & x,const tdeg_t14 & y,order_t order){
    int res=0;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    longlong tmp=xtab[0]-ytab[0];
    if (tmp & mask2){
      if (res==1 || ((-tmp) & mask2)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    tmp=xtab[1]-ytab[1];
    if (tmp & mask2){
      if (res==1 || ((-tmp) & mask2)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    return res;
  }

  void index_lcm(const tdeg_t14 & x,const tdeg_t14 & y,tdeg_t14 & z,order_t order){
    int t=0;
    const unsigned char * xtab=&x.tab[2],*ytab=&y.tab[2];
    unsigned char *ztab=&z.tab[2];
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 2
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 3
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 4
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 5
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 6
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 7
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 8
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 12
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
    if (t>=128){
      gensizeerr("Degree too large");
    }
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
      z.tab[0]=t;
    }
    else {
      z.tab[0]=(x.tab[0]>y.tab[0])?x.tab[0]:y.tab[0];
    }
  }

  void index_lcm_overwrite(const tdeg_t14 & x,const tdeg_t14 & y,tdeg_t14 & z,order_t order){
    index_lcm(x,y,z,order);
  }
  
  void get_index(const tdeg_t14 & x_,index_t & idx,order_t order,int dim){
    idx.resize(dim);
#ifdef GBASIS_SWAP    
    tdeg_t14 x(x_);
    swap_indices14(x.tab);
#else
    const tdeg_t14 & x= x_;
#endif
    const unsigned char * ptr=x.tab+2;
    if (order.o==_REVLEX_ORDER){
      for (int i=1;i<=dim;++ptr,++i)
	idx[dim-i]=*ptr;
    }
    else {
      for (int i=0;i<dim;++ptr,++i)
	idx[i]=*ptr;
    }
  }
  
  bool disjoint(const tdeg_t14 & a,const tdeg_t14 & b,order_t order,short dim){
    const unsigned char * it=a.tab+2, * jt=b.tab+2;
#ifdef GBASIS_SWAP
    const unsigned char * itend=it+14;
#else
    const unsigned char * itend=it+dim;
#endif
    for (;it<itend;++jt,++it){
      if (*it && *jt)
	return false;
    }
    return true;
  }
#endif

#undef GROEBNER_VARS
#define GROEBNER_VARS 11

  struct tdeg_t11 {
    bool vars64() const { return false;}
    int hash_index(void * ptr_) const {
      // if (!ptr_)
	return -1;
    }
    bool add_to_hash(void *ptr_,int no) const {
      return false;
    }
    void dbgprint() const;
    // data
    union {
      short tab[GROEBNER_VARS+1];
      struct {
	short tdeg; // actually it's twice the total degree+1
	short tdeg2;
	order_t order_;
	longlong * ui;
      };
    };
    int front(){ return tab[1];}
    // methods
    inline unsigned selection_degree(order_t order) const {
      return tab[0];
    }
    inline unsigned total_degree(order_t order) const {
      // works only for revlex and tdeg
      return tab[0];
    }
    // void set_total_degree(unsigned d) { tab[0]=d;}
    tdeg_t11() { 
      longlong * ptr = (longlong *) tab;
      ptr[2]=ptr[1]=ptr[0]=0;
    }
    tdeg_t11(int i){
      longlong * ptr = (longlong *) tab;
      ptr[2]=ptr[1]=ptr[0]=0;
    }
    void get_tab(short * ptr,order_t order) const {
      for (unsigned i=0;i<=11;++i)
	ptr[i]=tab[i];
#ifdef GBASIS_SWAP
      swap_indices11(ptr);
#endif
      ptr[15]=ptr[14]=ptr[13]=ptr[12]=0;
    }
    tdeg_t11(const index_m & lm,order_t order){ 
      longlong * ptr_ = (longlong *) tab;
      ptr_[2]=ptr_[1]=ptr_[0]=0;
      short * ptr=tab;
      // tab[GROEBNER_VARS]=order;
      vector<deg_t>::const_iterator it=lm.begin(),itend=lm.end();
      if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
	*ptr=sum_degree(lm);
	++ptr;
      }
      if (order.o==_REVLEX_ORDER){
	for (--itend,--it;it!=itend;++ptr,--itend)
	  *ptr=*itend;
      }
      else {
	for (;it!=itend;++ptr,++it)
	  *ptr=*it;
      }
#ifdef GBASIS_SWAP
      swap_indices11(tab);
#endif
    }
  };


#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const tdeg_t11 & x){
    os << "[";
    for (unsigned i=0; i<=GROEBNER_VARS;++i){
      os << x.tab[i] << ",";
    }
    return os << "]";
  }
#else
  ostream & operator << (ostream & os,const tdeg_t11 & x){
    os << "[";
    for (unsigned i=0; i<=GROEBNER_VARS;++i){
      os << x.tab[i] << ",";
    }
    return os << "]";
  }
#endif
  void tdeg_t11::dbgprint() const { COUT << * this << '\n'; }
  tdeg_t11 operator + (const tdeg_t11 & x,const tdeg_t11 & y);
  tdeg_t11 & operator += (tdeg_t11 & x,const tdeg_t11 & y){ 
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    xtab[0]+=ytab[0];
    xtab[1]+=ytab[1];
    xtab[2]+=ytab[2];
    return x;  
  }
  tdeg_t11 operator + (const tdeg_t11 & x,const tdeg_t11 & y){
    tdeg_t11 res(x);
    return res += y;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]+ytab[0];
    ztab[1]=xtab[1]+ytab[1];
    ztab[2]=xtab[2]+ytab[2];
    return res;
  }
  inline void add(const tdeg_t11 & x,const tdeg_t11 & y,tdeg_t11 & res,int dim){
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]+ytab[0];
    ztab[1]=xtab[1]+ytab[1];
    ztab[2]=xtab[2]+ytab[2];
  }
  tdeg_t11 operator - (const tdeg_t11 & x,const tdeg_t11 & y){ 
    tdeg_t11 res;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]-ytab[0];
    ztab[1]=xtab[1]-ytab[1];
    ztab[2]=xtab[2]-ytab[2];
    return res;
  }
  inline bool operator == (const tdeg_t11 & x,const tdeg_t11 & y){ 
    return ((longlong *) x.tab)[0] == ((longlong *) y.tab)[0] &&
      ((longlong *) x.tab)[1] == ((longlong *) y.tab)[1] &&
      ((longlong *) x.tab)[2] == ((longlong *) y.tab)[2] 
    ;
  }
  inline bool operator != (const tdeg_t11 & x,const tdeg_t11 & y){ 
    return !(x==y);
  }

  static inline int tdeg_t11_revlex_greater (const tdeg_t11 & x,const tdeg_t11 & y){
#ifdef GBASIS_SWAP
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    if (xtab[0]!=ytab[0]) // tdeg test already donne by caller
      return xtab[0]<=ytab[0]?1:0;
    if (xtab[1]!=ytab[1])
      return xtab[1]<=ytab[1]?1:0;
    if (xtab[2]!=ytab[2])
      return xtab[2]<=ytab[2]?1:0;
#else
    longlong *xtab=(longlong *)&x,*ytab=(longlong *)&y;
    if (longlong a=*xtab-*ytab) // tdeg test already donne by caller
      return a<=0?1:0;
    if (longlong a=xtab[1]-ytab[1])
      return a<=0?1:0;
    if (longlong a=xtab[2]-ytab[2])
      return a<=0?1:0;
#endif
    return 2;
#else // GBASIS_SWAP
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>=y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]<=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
    }
    return 2;
#endif // GBASIS_SWAP
  }

  int tdeg_t11_lex_greater (const tdeg_t11 & x,const tdeg_t11 & y){
#ifdef GBASIS_SWAP
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    ulonglong X=*xtab, Y=*ytab;
    if (X!=Y){
      if ( (X & 0xffff) != (Y &0xffff))
	return (X&0xffff)>=(Y&0xffff)?1:0;
      return X>=Y?1:0;
    }
    if (xtab[1]!=ytab[1])
      return xtab[1]>=ytab[1]?1:0;
    if (xtab[2]!=ytab[2])
      return xtab[2]>=ytab[2]?1:0;
    return 2;
#else
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]>y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]>y.tab[2]?1:0;
      return x.tab[3]>y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]>y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]>y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]>y.tab[6]?1:0;
      return x.tab[7]>y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]>y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]>y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]>y.tab[10]?1:0;
      return x.tab[11]>=y.tab[11]?1:0;
    }
    return 2;
#endif
  }

  inline int tdeg_t_greater(const tdeg_t11 & x,const tdeg_t11 & y,order_t order){
    short X=x.tab[0];
    if (//order.o!=_PLEX_ORDER &&
        X!=y.tab[0]) return X>y.tab[0]?1:0; // since tdeg is tab[0] for plex
    if (order.o==_REVLEX_ORDER)
      return tdeg_t11_revlex_greater(x,y);
    return tdeg_t11_lex_greater(x,y);
  }
  inline bool tdeg_t_strictly_greater (const tdeg_t11 & x,const tdeg_t11 & y,order_t order){
    return !tdeg_t_greater(y,x,order); // total order
  }

  inline bool tdeg_t_all_greater(const tdeg_t11 & x,const tdeg_t11 & y,order_t order){
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    if ((xtab[0]-ytab[0]) & 0x8000800080008000ULL)
      return false;
    if ((xtab[1]-ytab[1]) & 0x8000800080008000ULL)
      return false;
    if ((xtab[2]-ytab[2]) & 0x8000800080008000ULL)
      return false;
    return true;
  }

  // 1 (all greater), 0 (unknown), -1 (all smaller)
  int tdeg_t_compare_all(const tdeg_t11 & x,const tdeg_t11 & y,order_t order){
    int res=0;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    longlong tmp=xtab[0]-ytab[0];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    tmp=xtab[1]-ytab[1];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    tmp=xtab[2]-ytab[2];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    return res;
  }

  void index_lcm(const tdeg_t11 & x,const tdeg_t11 & y,tdeg_t11 & z,order_t order){
    int t=0;
    const short * xtab=&x.tab[1],*ytab=&y.tab[1];
    short *ztab=&z.tab[1];
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 1
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 2
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 3
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 4
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 5
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 6
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 7
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 8
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
      z.tab[0]=t;
    }
    else {
      z.tab[0]=(x.tab[0]>y.tab[0])?x.tab[0]:y.tab[0];
    }
  }

  inline void index_lcm_overwrite(const tdeg_t11 & x,const tdeg_t11 & y,tdeg_t11 & z,order_t order){
    index_lcm(x,y,z,order);
  }
  
  void get_index(const tdeg_t11 & x_,index_t & idx,order_t order,int dim){
    idx.resize(dim);
#ifdef GBASIS_SWAP    
    tdeg_t11 x(x_);
    swap_indices11(x.tab);
#else
    const tdeg_t11 & x= x_;
#endif
    const short * ptr=x.tab;
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER)
      ++ptr;
    if (order.o==_REVLEX_ORDER){
      for (int i=1;i<=dim;++ptr,++i)
	idx[dim-i]=*ptr;
    }
    else {
      for (int i=0;i<dim;++ptr,++i)
	idx[i]=*ptr;
    }
  }
  
  bool disjoint(const tdeg_t11 & a,const tdeg_t11 & b,order_t order,short dim){
    const short * it=a.tab, * jt=b.tab;
#ifdef GBASIS_SWAP
    const short * itend=it+GROEBNER_VARS+1;
#endif
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
      ++it; ++jt;
    }
#ifndef GBASIS_SWAP
    const short * itend=it+dim;
#endif
    for (;it<itend;++jt,++it){
      if (*it && *jt)
	return false;
    }
    return true;
  }

#undef GROEBNER_VARS
#define GROEBNER_VARS 15
  
  struct tdeg_t15 {
    bool vars64() const { return false;}
    int hash_index(void * ptr_) const {
      // if (!ptr_)
	return -1;
    }
    bool add_to_hash(void *ptr_,int no) const {
      return false;
    }
    void dbgprint() const;
    // data
    union {
      short tab[GROEBNER_VARS+1];
      struct {
	short tdeg; // actually it's twice the total degree+1
	short tdeg2;
	order_t order_;
	longlong * ui;
      };
    };
    int front(){ return tab[1];}
    // methods
    inline unsigned selection_degree(order_t order) const {
#ifdef GBASIS_SELECT_TOTAL_DEGREE
      return total_degree(order);
#endif
      return tab[0];
    }
    inline unsigned total_degree(order_t order) const {
      if (order.o==_REVLEX_ORDER)
	return tab[0];      // works only for revlex and tdeg
      if (order.o==_3VAR_ORDER)
	return tab[0]+tab[4];
      if (order.o==_7VAR_ORDER)
	return tab[0]+tab[8];
      if (order.o==_11VAR_ORDER)
	return tab[0]+tab[12];
      return tab[0];
    }
    // void set_total_degree(unsigned d) { tab[0]=d;}
    tdeg_t15() { 
      longlong * ptr = (longlong *) tab;
      ptr[2]=ptr[1]=ptr[0]=0;
#if GROEBNER_VARS>11
      ptr[3]=0;
#endif
    }
    tdeg_t15(int i){
      longlong * ptr = (longlong *) tab;
      ptr[2]=ptr[1]=ptr[0]=0;
#if GROEBNER_VARS>11
      ptr[3]=0;
#endif
    }
    void get_tab(short * ptr,order_t order) const {
      for (unsigned i=0;i<=GROEBNER_VARS;++i)
	ptr[i]=tab[i];
#ifdef GBASIS_SWAP
      swap_indices15(ptr,order.o);
#endif
    }
    tdeg_t15(const index_m & lm,order_t order){ 
      longlong * ptr_ = (longlong *) tab;
      ptr_[2]=ptr_[1]=ptr_[0]=0;
      short * ptr=tab;
#if GROEBNER_VARS>11
      ptr_[3]=0;
#endif
      // tab[GROEBNER_VARS]=order;
#if GROEBNER_VARS==15
      if (order.o==_3VAR_ORDER){
	ptr[0]=lm[0]+lm[1]+lm[2];
	ptr[1]=lm[2];
	ptr[2]=lm[1];
	ptr[3]=lm[0];
	ptr +=5;
	short t=0;
	vector<deg_t>::const_iterator it=lm.begin()+3,itend=lm.end();
	for (--itend,--it;it!=itend;++ptr,--itend){
	  t += *itend;
	  *ptr=*itend;
	}
	tab[4]=t;
#ifdef GBASIS_SWAP
	swap_indices15(tab,order.o);
#endif
	return;
      }
      if (order.o==_7VAR_ORDER){
	ptr[0]=lm[0]+lm[1]+lm[2]+lm[3]+lm[4]+lm[5]+lm[6];
	ptr[1]=lm[6];
	ptr[2]=lm[5];
	ptr[3]=lm[4];
	ptr[4]=lm[3];
	ptr[5]=lm[2];
	ptr[6]=lm[1];
	ptr[7]=lm[0];
	ptr +=9;
	short t=0;
	vector<deg_t>::const_iterator it=lm.begin()+7,itend=lm.end();
	for (--itend,--it;it!=itend;++ptr,--itend){
	  t += *itend;
	  *ptr=*itend;
	}
	tab[8]=t;
#ifdef GBASIS_SWAP
	swap_indices15(tab,order.o);
#endif
	return;
      }
      if (order.o==_11VAR_ORDER){
	ptr[0]=lm[0]+lm[1]+lm[2]+lm[3]+lm[4]+lm[5]+lm[6]+lm[7]+lm[8]+lm[9]+lm[10];
	ptr[1]=lm[10];
	ptr[2]=lm[9];
	ptr[3]=lm[8];
	ptr[4]=lm[7];
	ptr[5]=lm[6];
	ptr[6]=lm[5];
	ptr[7]=lm[4];
	ptr[8]=lm[3];
	ptr[9]=lm[2];
	ptr[10]=lm[1];
	ptr[11]=lm[0];
	ptr += 13;
	short t=0;
	vector<deg_t>::const_iterator it=lm.begin()+11,itend=lm.end();
	for (--itend,--it;it!=itend;++ptr,--itend){
	  t += *itend;
	  *ptr=*itend;
	}
	tab[12]=t;
#ifdef GBASIS_SWAP
	swap_indices15(tab,order.o);
#endif
	return;
      }
#endif
      vector<deg_t>::const_iterator it=lm.begin(),itend=lm.end();
      if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
	*ptr=sum_degree(lm);
	++ptr;
      }
      if (order.o==_REVLEX_ORDER){
	for (--itend,--it;it!=itend;++ptr,--itend)
	  *ptr=*itend;
      }
      else {
	for (;it!=itend;++ptr,++it)
	  *ptr=*it;
      }
#ifdef GBASIS_SWAP
      swap_indices15(tab,order.o);
#endif
    }
  };


#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const tdeg_t15 & x){
    os << "[";
    for (unsigned i=0; i<=GROEBNER_VARS;++i){
      os << x.tab[i] << ",";
    }
    return os << "]";
  }
#else
  ostream & operator << (ostream & os,const tdeg_t15 & x){
    os << "[";
    for (unsigned i=0; i<=GROEBNER_VARS;++i){
      os << x.tab[i] << ",";
    }
    return os << "]";
  }
#endif
  void tdeg_t15::dbgprint() const { COUT << * this << '\n'; }
  tdeg_t15 operator + (const tdeg_t15 & x,const tdeg_t15 & y);
  tdeg_t15 & operator += (tdeg_t15 & x,const tdeg_t15 & y){ 
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    xtab[0]+=ytab[0];
    xtab[1]+=ytab[1];
    xtab[2]+=ytab[2];
#if GROEBNER_VARS>11
    xtab[3]+=ytab[3];
#endif
    return x;  
  }
  tdeg_t15 operator + (const tdeg_t15 & x,const tdeg_t15 & y){
    tdeg_t15 res(x);
    return res += y;
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]+ytab[0];
    ztab[1]=xtab[1]+ytab[1];
    ztab[2]=xtab[2]+ytab[2];
#if GROEBNER_VARS>11
    ztab[3]=xtab[3]+ytab[3];
#endif
#else
    for (unsigned i=0;i<=GROEBNER_VARS;++i)
      res.tab[i]=x.tab[i]+y.tab[i];
#endif
    return res;
  }
  inline void add(const tdeg_t15 & x,const tdeg_t15 & y,tdeg_t15 & res,int dim){
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]+ytab[0];
    ztab[1]=xtab[1]+ytab[1];
    ztab[2]=xtab[2]+ytab[2];
#if GROEBNER_VARS>11
    ztab[3]=xtab[3]+ytab[3];
#endif
#else
    for (unsigned i=0;i<=dim;++i)
      res.tab[i]=x.tab[i]+y.tab[i];
#endif
  }
  tdeg_t15 operator - (const tdeg_t15 & x,const tdeg_t15 & y){ 
    tdeg_t15 res;
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y,*ztab=(ulonglong *)&res;
    ztab[0]=xtab[0]-ytab[0];
    ztab[1]=xtab[1]-ytab[1];
    ztab[2]=xtab[2]-ytab[2];
#if GROEBNER_VARS>11
    ztab[3]=xtab[3]-ytab[3];
#endif
#else
    for (unsigned i=0;i<=GROEBNER_VARS;++i)
      res.tab[i]=x.tab[i]-y.tab[i];
#endif
    return res;
  }
  inline bool operator == (const tdeg_t15 & x,const tdeg_t15 & y){ 
    return ((longlong *) x.tab)[0] == ((longlong *) y.tab)[0] &&
      ((longlong *) x.tab)[1] == ((longlong *) y.tab)[1] &&
      ((longlong *) x.tab)[2] == ((longlong *) y.tab)[2] 
#if GROEBNER_VARS>11
      &&  ((longlong *) x.tab)[3] == ((longlong *) y.tab)[3]
#endif
    ;
  }
  inline bool operator != (const tdeg_t15 & x,const tdeg_t15 & y){ 
    return !(x==y);
  }

  static inline int tdeg_t15_revlex_greater (const tdeg_t15 & x,const tdeg_t15 & y){
#ifdef GBASIS_SWAP
#if 1
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    if (xtab[0]!=ytab[0]) // tdeg test already donne by caller
      return xtab[0]<=ytab[0]?1:0;
    if (xtab[1]!=ytab[1])
      return xtab[1]<=ytab[1]?1:0;
    if (xtab[2]!=ytab[2])
      return xtab[2]<=ytab[2]?1:0;
#else
    longlong *xtab=(longlong *)&x,*ytab=(longlong *)&y;
    if (longlong a=*xtab-*ytab) // tdeg test already donne by caller
      return a<=0?1:0;
    if (longlong a=xtab[1]-ytab[1])
      return a<=0?1:0;
    if (longlong a=xtab[2]-ytab[2])
      return a<=0?1:0;
#endif
#if GROEBNER_VARS>11
    return xtab[3]<=ytab[3]?1:0;
#endif
    return 2;
#else // GBASIS_SWAP
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>=y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]<=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
    }
#if GROEBNER_VARS>11
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]<=y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
    }
#endif
    return 2;
#endif // GBASIS_SWAP
  }


#if GROEBNER_VARS==15

  int tdeg_t15_3var_greater (const tdeg_t15 & x,const tdeg_t15 & y){
    if (x.tab[0]!=y.tab[0])
      return x.tab[0]>=y.tab[0]?1:0;
    if (x.tab[4]!=y.tab[4])
      return x.tab[4]>=y.tab[4]?1:0;
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[0] <= ((longlong *) y.tab)[0];
#else
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
#endif
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[1] <= ((longlong *) y.tab)[1];
#else
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
#endif
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[2] <= ((longlong *) y.tab)[2];
#else
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]<=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
#endif
    }
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[3] <= ((longlong *) y.tab)[3];
#else
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]<=y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
#endif
    }
    return 2;
  }

  int tdeg_t15_7var_greater (const tdeg_t15 & x,const tdeg_t15 & y){
    if (x.tab[0]!=y.tab[0])
      return x.tab[0]>=y.tab[0]?1:0;
    if (x.tab[8]!=y.tab[8])
      return x.tab[8]>=y.tab[8]?1:0;
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[0] <= ((longlong *) y.tab)[0];
#else
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
#endif
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[1] <= ((longlong *) y.tab)[1];
#else
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
#endif
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[2] <= ((longlong *) y.tab)[2];
#else
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
#endif
    }
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[3] <= ((longlong *) y.tab)[3];
#else
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]<=y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
#endif
    }
    return 2;
  }

  int tdeg_t15_11var_greater (const tdeg_t15 & x,const tdeg_t15 & y){
    if (x.tab[0]!=y.tab[0])
      return x.tab[0]>=y.tab[0]?1:0;
    if (x.tab[12]!=y.tab[12])
      return x.tab[12]>=y.tab[12]?1:0;
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[0] <= ((longlong *) y.tab)[0];
#else
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]<=y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]<=y.tab[2]?1:0;
      return x.tab[3]<=y.tab[3]?1:0;
#endif
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[1] <= ((longlong *) y.tab)[1];
#else
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]<=y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]<=y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]<=y.tab[6]?1:0;
      return x.tab[7]<=y.tab[7]?1:0;
#endif
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[2] <= ((longlong *) y.tab)[2];
#else
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]<=y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]<=y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]<=y.tab[10]?1:0;
      return x.tab[11]<=y.tab[11]?1:0;
#endif
    }
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
#ifdef GBASIS_SWAP
      return ((longlong *) x.tab)[3] <= ((longlong *) y.tab)[3];
#else
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]<=y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]<=y.tab[14]?1:0;
      return x.tab[15]<=y.tab[15]?1:0;
#endif
    }
    return 2;
  }
#endif // GROEBNER_VARS==15

  int tdeg_t15_lex_greater (const tdeg_t15 & x,const tdeg_t15 & y){
#if 0 // def GBASIS_SWAP
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    ulonglong X=*xtab, Y=*ytab;
    if (X!=Y){
      if ( (X & 0xffff) != (Y &0xffff))
	return (X&0xffff)>=(Y&0xffff)?1:0;
      return X>=Y?1:0;
    }
    if (xtab[1]!=ytab[1])
      return xtab[1]>=ytab[1]?1:0;
    if (xtab[2]!=ytab[2])
      return xtab[2]>=ytab[2]?1:0;
#if GROEBNER_VARS>11
    return xtab[3]>=ytab[3]?1:0;
#endif
    return 2;
#else
    if (((longlong *) x.tab)[0] != ((longlong *) y.tab)[0]){
      if (x.tab[0]!=y.tab[0])
	return x.tab[0]>y.tab[0]?1:0;
      if (x.tab[1]!=y.tab[1])
	return x.tab[1]>y.tab[1]?1:0;
      if (x.tab[2]!=y.tab[2])
	return x.tab[2]>y.tab[2]?1:0;
      return x.tab[3]>y.tab[3]?1:0;
    }
    if (((longlong *) x.tab)[1] != ((longlong *) y.tab)[1]){
      if (x.tab[4]!=y.tab[4])
	return x.tab[4]>y.tab[4]?1:0;
      if (x.tab[5]!=y.tab[5])
	return x.tab[5]>y.tab[5]?1:0;
      if (x.tab[6]!=y.tab[6])
	return x.tab[6]>y.tab[6]?1:0;
      return x.tab[7]>y.tab[7]?1:0;
    }
    if (((longlong *) x.tab)[2] != ((longlong *) y.tab)[2]){
      if (x.tab[8]!=y.tab[8])
	return x.tab[8]>y.tab[8]?1:0;
      if (x.tab[9]!=y.tab[9])
	return x.tab[9]>y.tab[9]?1:0;
      if (x.tab[10]!=y.tab[10])
	return x.tab[10]>y.tab[10]?1:0;
      return x.tab[11]>=y.tab[11]?1:0;
    }
#if GROEBNER_VARS>11
    if (((longlong *) x.tab)[3] != ((longlong *) y.tab)[3]){
      if (x.tab[12]!=y.tab[12])
	return x.tab[12]>y.tab[12]?1:0;
      if (x.tab[13]!=y.tab[13])
	return x.tab[13]>y.tab[13]?1:0;
      if (x.tab[14]!=y.tab[14])
	return x.tab[14]>y.tab[14]?1:0;
      return x.tab[15]>=y.tab[15]?1:0;
    }
#endif
    return 2;
#endif
  }

  inline int tdeg_t_greater(const tdeg_t15 & x,const tdeg_t15 & y,order_t order){
    short X=x.tab[0];
    if (//order.o!=_PLEX_ORDER &&
        X!=y.tab[0]) return X>y.tab[0]?1:0; // since tdeg is tab[0] for plex
    if (order.o==_REVLEX_ORDER)
      return tdeg_t15_revlex_greater(x,y);
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER)
      return tdeg_t15_3var_greater(x,y);
    if (order.o==_7VAR_ORDER)
      return tdeg_t15_7var_greater(x,y);
    if (order.o==_11VAR_ORDER)
      return tdeg_t15_11var_greater(x,y);
#endif
    return tdeg_t15_lex_greater(x,y);
  }
  inline bool tdeg_t_strictly_greater (const tdeg_t15 & x,const tdeg_t15 & y,order_t order){
    return !tdeg_t_greater(y,x,order); // total order
  }

  bool tdeg_t_all_greater(const tdeg_t15 & x,const tdeg_t15 & y,order_t order){
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    if ((xtab[0]-ytab[0]) & 0x8000800080008000ULL)
      return false;
    if ((xtab[1]-ytab[1]) & 0x8000800080008000ULL)
      return false;
    if ((xtab[2]-ytab[2]) & 0x8000800080008000ULL)
      return false;
#if GROEBNER_VARS>11
    if ((xtab[3]-ytab[3]) & 0x8000800080008000ULL)
      return false;
#endif
    return true;
  }

  // 1 (all greater), 0 (unknown), -1 (all smaller)
  int tdeg_t_compare_all(const tdeg_t15 & x,const tdeg_t15 & y,order_t order){
    int res=0;
    ulonglong *xtab=(ulonglong *)&x,*ytab=(ulonglong *)&y;
    longlong tmp=xtab[0]-ytab[0];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    tmp=xtab[1]-ytab[1];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
    tmp=xtab[2]-ytab[2];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
#if GROEBNER_VARS>11
    tmp=xtab[3]-ytab[3];
    if (tmp & mask){
      if (res==1 || ((-tmp) & mask)) return 0;
      res=-1;
    }
    else {
      if (res==-1) return 0; else res=1;
    }
#endif
    return res;
  }

  void index_lcm(const tdeg_t15 & x,const tdeg_t15 & y,tdeg_t15 & z,order_t order){
    int t=0;
    const short * xtab=&x.tab[1],*ytab=&y.tab[1];
    short *ztab=&z.tab[1];
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 1
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 2
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 3
    ++xtab; ++ytab; ++ztab;
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER){
      z.tab[0]=t;
      t=0;
      ++xtab;++ytab;++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 5
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 6
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 7
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 8
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 12
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
      z.tab[4]=t; // 4
      return;
    }
#endif
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 4
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 5
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 6
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 7
    ++xtab; ++ytab; ++ztab;
#if GROEBNER_VARS==15
    if (order.o==_7VAR_ORDER){
      z.tab[0]=t;
      t=0;
      ++xtab;++ytab;++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 12
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
      z.tab[8]=t; // 8
      return;
    }
#endif
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 8
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 9
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 10
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 11
#if GROEBNER_VARS>11
    ++xtab; ++ytab; ++ztab;
#if GROEBNER_VARS==15
    if (order.o==_11VAR_ORDER){
      z.tab[0]=t;
      t=0;
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
      ++xtab; ++ytab; ++ztab;
      t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
      z.tab[12]=t; // 12
      return;
    }
#endif
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 12
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 13
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 14
    ++xtab; ++ytab; ++ztab;
    t += (*ztab=(*xtab>*ytab)?*xtab:*ytab); // 15
#endif
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
      z.tab[0]=t;
    }
    else {
      z.tab[0]=(x.tab[0]>y.tab[0])?x.tab[0]:y.tab[0];
    }
  }

  inline void index_lcm_overwrite(const tdeg_t15 & x,const tdeg_t15 & y,tdeg_t15 & z,order_t order){
    index_lcm(x,y,z,order);
  }  

  void get_index(const tdeg_t15 & x_,index_t & idx,order_t order,int dim){
    idx.resize(dim);
#ifdef GBASIS_SWAP    
    tdeg_t15 x(x_);
    swap_indices15(x.tab,order.o);
#else
    const tdeg_t15 & x= x_;
#endif
    const short * ptr=x.tab;
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER){
      ++ptr;
      for (int i=1;i<=3;++ptr,++i)
	idx[3-i]=*ptr;
      ++ptr;
      for (int i=1;i<=dim-3;++ptr,++i)
	idx[dim-i]=*ptr;
      return;
    }
    if (order.o==_7VAR_ORDER){
      ++ptr;
      for (int i=1;i<=7;++ptr,++i)
	idx[7-i]=*ptr;
      ++ptr;
      for (int i=1;i<=dim-7;++ptr,++i)
	idx[dim-i]=*ptr;
      return;
    }
    if (order.o==_11VAR_ORDER){
      ++ptr;
      for (int i=1;i<=11;++ptr,++i)
	idx[11-i]=*ptr;
      ++ptr;
      for (int i=1;i<=dim-11;++ptr,++i)
	idx[dim-i]=*ptr;
      return;
    }
#endif
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER)
      ++ptr;
    if (order.o==_REVLEX_ORDER){
      for (int i=1;i<=dim;++ptr,++i)
	idx[dim-i]=*ptr;
    }
    else {
      for (int i=0;i<dim;++ptr,++i)
	idx[i]=*ptr;
    }
  }
  
  bool disjoint(const tdeg_t15 & a,const tdeg_t15 & b,order_t order,short dim){
#if GROEBNER_VARS==15
    if (order.o==_3VAR_ORDER){
      if ( (a.tab[1] && b.tab[1]) ||
	   (a.tab[2] && b.tab[2]) ||
	   (a.tab[3] && b.tab[3]) ||
	   (a.tab[5] && b.tab[5]) ||
	   (a.tab[6] && b.tab[6]) ||
	   (a.tab[7] && b.tab[7]) ||
	   (a.tab[8] && b.tab[8]) ||
	   (a.tab[9] && b.tab[9]) ||
	   (a.tab[10] && b.tab[10]) ||
	   (a.tab[11] && b.tab[11]) ||
	   (a.tab[12] && b.tab[12]) ||
	   (a.tab[13] && b.tab[13]) ||
	   (a.tab[14] && b.tab[14]) ||
	   (a.tab[15] && b.tab[15]) )
	return false;
      return true;
    }
    if (order.o==_7VAR_ORDER){
      if ( (a.tab[1] && b.tab[1]) ||
	   (a.tab[2] && b.tab[2]) ||
	   (a.tab[3] && b.tab[3]) ||
	   (a.tab[4] && b.tab[4]) ||
	   (a.tab[5] && b.tab[5]) ||
	   (a.tab[6] && b.tab[6]) ||
	   (a.tab[7] && b.tab[7]) ||
	   (a.tab[9] && b.tab[9]) ||
	   (a.tab[10] && b.tab[10]) ||
	   (a.tab[11] && b.tab[11]) ||
	   (a.tab[12] && b.tab[12]) ||
	   (a.tab[13] && b.tab[13]) ||
	   (a.tab[14] && b.tab[14]) ||
	   (a.tab[15] && b.tab[15]) )
	return false;
      return true;
    }
    if (order.o==_11VAR_ORDER){
      if ( (a.tab[1] && b.tab[1]) ||
	   (a.tab[2] && b.tab[2]) ||
	   (a.tab[3] && b.tab[3]) ||
	   (a.tab[4] && b.tab[4]) ||
	   (a.tab[5] && b.tab[5]) ||
	   (a.tab[6] && b.tab[6]) ||
	   (a.tab[7] && b.tab[7]) ||
	   (a.tab[8] && b.tab[8]) ||
	   (a.tab[9] && b.tab[9]) ||
	   (a.tab[10] && b.tab[10]) ||
	   (a.tab[11] && b.tab[11]) ||
	   (a.tab[13] && b.tab[13]) ||
	   (a.tab[14] && b.tab[14]) ||
	   (a.tab[15] && b.tab[15]))
	return false;
      return true;
    }
#endif
    const short * it=a.tab, * jt=b.tab;
#ifdef GBASIS_SWAP
    const short * itend=it+GROEBNER_VARS+1;
#endif
    if (order.o==_REVLEX_ORDER || order.o==_TDEG_ORDER){
      ++it; ++jt;
    }
#ifndef GBASIS_SWAP
    const short * itend=it+dim;
#endif
    for (;it<itend;++jt,++it){
      if (*it && *jt)
	return false;
    }
    return true;
  }

  template<class T>
  static void get_newres(const T & resmod,vectpoly & newres,const vectpoly & v,const vector<unsigned> & G){
    newres=vectpoly(G.size(),polynome(v.front().dim,v.front()));
    for (unsigned i=0;i<G.size();++i)
      resmod[G[i]].get_polynome(newres[i]);
  }

  template<class tdeg_t>
  static void get_newres(const vectpoly8<tdeg_t> & resmod,vectpoly & newres,const vectpoly & v,vector< vectpoly8<tdeg_t> > * coeffsmodptr,vector<vectpoly>  * coeffsptr){
    newres=vectpoly(resmod.size(),polynome(v.front().dim,v.front()));
    for (unsigned i=0;i<resmod.size();++i)
      resmod[i].get_polynome(newres[i]);
    if (coeffsmodptr && coeffsptr){
      coeffsptr->clear();
      coeffsptr->resize(resmod.size());
      for (unsigned i=0;i<resmod.size();++i){
        const vectpoly8<tdeg_t> & src = (*coeffsmodptr)[i];
        vectpoly & target = (*coeffsptr)[i];
	target.resize(src.size());
        for (unsigned j=0;j<src.size();++j)
          src[j].get_polynome(target[j]);
      }
    }
  }

  template<class tdeg_t,class modint_t>
  static void get_newres_ckrur(const vectpolymod<tdeg_t,modint_t> & resmod,vectpoly & newres,const vectpoly & v,const vector<unsigned> & G,modint env,int & rur,vector<int> *initsep,vector< vectpolymod<tdeg_t,modint_t> > * coeffsmodptr,vector<vectpoly>  * coeffsptr){
    if (rur && !resmod.empty()){
      vectpolymod<tdeg_t,modint_t> gbmod; gbmod.reserve(G.size());
      for (int i=0;i<int(G.size());++i)
	gbmod.push_back(resmod[G[i]]);
      order_t order=resmod.front().order; int dim=resmod.front().dim;
      polymod<tdeg_t,modint_t> lmtmp(order,dim),lmmodradical(order,dim);
      polymod<tdeg_t,modint> s(order,dim);
      vectpolymod<tdeg_t,modint_t> rurv;
      if (rur_quotient_ideal_dimension(gbmod,lmtmp)<0)
	rur=0;
      else {
	if (debug_infolevel)
	  CERR << CLOCK()*1e-6 << " begin modular rur computation" << '\n';
	bool ok;
	if (rur==2){
	  vecteur m,M,res;
	  s.coord.clear();
	  index_t l(dim);
	  l[dim-1]=1;
	  s.coord.push_back(T_unsigned<modint,tdeg_t>(1,tdeg_t(l,order)));
	  ok=rur_minpoly<tdeg_t,modint,modint2>(gbmod,lmtmp,s,env,m,M);
	  rur_convert_univariate(m,dim-1,gbmod[0]);
	  gbmod.resize(1);
	}
	else {
	  ok=rur_compute<tdeg_t>(gbmod,lmtmp,lmmodradical,env,s,initsep,rurv);
	  if (ok)
	    gbmod.swap(rurv);
	}
	if (!ok)
	  rur=0;
      }
      if (debug_infolevel)
	CERR << CLOCK()*1e-6 << " end modular rur computation" << '\n';
      newres=vectpoly(gbmod.size(),polynome(v.front().dim,v.front()));
      for (unsigned i=0;i<int(gbmod.size());++i){
	gbmod[i].get_polynome(newres[i]);
	if (env){
	  for (int j=0;j<gbmod[i].coord.size();++j)
	    makepositive(gbmod[i].coord[j].g,env);
	}
      }
      return;
    }
    newres=vectpoly(G.size(),polynome(v.front().dim,v.front()));
    for (unsigned i=0;i<G.size();++i)
      resmod[G[i]].get_polynome(newres[i]);
    if (coeffsmodptr && coeffsptr){
      coeffsptr->clear();
      coeffsptr->resize(G.size());
      for (unsigned i=0;i<G.size();++i){
        const vectpolymod<tdeg_t,modint_t> & src = (*coeffsmodptr)[G[i]];
        vectpoly & target = (*coeffsptr)[i];
	target.resize(src.size());
        for (unsigned j=0;j<src.size();++j)
          src[j].get_polynome(target[j]);
      }
    }
  }

  bool gbasis8(const vectpoly & v,order_t & order,vectpoly & newres,environment * env,bool modularalgo,bool modularcheck,int & rur,GIAC_CONTEXT,gbasis_param_t gbasis_param,vector<vectpoly> * coeffsptr){
    bool & eliminate_flag=gbasis_param.eliminate_flag;
    if (gbasis_param.buchberger_select_strategy==-1 && !v.empty()){
      if (GBASIS_COEFF_STRATEGY)
        gbasis_param.buchberger_select_strategy=GBASIS_COEFF_STRATEGY;
      else {
        gbasis_param.buchberger_select_strategy=(coeffsptr && v.front().dim<=10)?2/* topreduceonly=true */:11000;
        // gbasis_param.buchberger_select_strategy=(coeffsptr && v.front().dim<=8)?1000001/* topreduceonly=true */:0;
      }
      if (debug_infolevel)
        CERR << "strategy " << gbasis_param.buchberger_select_strategy << "\n";     }
    bool interred=gbasis_param.interred;
    int parallel=1;
#ifdef HAVE_LIBPTHREAD
    if (threads_allowed && threads>1)
      parallel=threads;
#endif
    int save_debuginfo=debug_infolevel;
    if (v.empty()){ newres.clear(); return true;}
#ifdef GIAC_TDEG_T14
    // do not use tdeg_t14 for rur because monomials have often large degrees
    if (v.front().dim<=14 && order.o==_REVLEX_ORDER && !rur){
      try {
	vectpoly8<tdeg_t14> res;
	vectpolymod<tdeg_t14,modint> resmod;
	vector<unsigned> G;
	vectpoly_2_vectpoly8<tdeg_t14>(v,order,res);
	// Temporary workaround until rur_compute support parametric rur
	if (rur && absint(order.o)!=-_RUR_REVLEX){ 
	  rur=0;
	  order.o=absint(order.o);
	}
	CLOCK_T c=CLOCK();
	if (modularalgo && (!env || env->modulo==0 || env->moduloon==false)){
          std::vector<giac::vectpoly8<tdeg_t14> > gbasis_coeffs;
	  if (mod_gbasis<tdeg_t14,qmodint,qmodint2>(res,modularcheck,
			 //order.o==_REVLEX_ORDER /* zdata*/,
			 1 || !rur /* zdata*/,
			 rur,contextptr,gbasis_param,coeffsptr?&gbasis_coeffs:0)){
            if (debug_infolevel)
              *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 << " Memory " << memory_usage()*1e-6 << 'M'<<'\n';
	    get_newres(res,newres,v,&gbasis_coeffs,coeffsptr);
	    debug_infolevel=save_debuginfo; return true;
	  }
	}
	if (env && env->moduloon && env->modulo.type==_INT_){
	  if (!res.empty() && (res.front().order.o==_REVLEX_ORDER || res.front().order.o==_3VAR_ORDER || res.front().order.o==_7VAR_ORDER || res.front().order.o==_11VAR_ORDER)){
	    vector<zinfo_t<tdeg_t14> > f4buchberger_info;
	    vector< vectpolymod<tdeg_t14,modint> > gbasiscoeff;
	    vector< paire > pairs_reducing_to_zero;
	    f4buchberger_info.reserve(GBASISF4_MAXITER);
	    if (zgbasis<tdeg_t14,modint,modint2>(res,resmod,G,env->modulo.val,true/*totaldeg*/,&pairs_reducing_to_zero,f4buchberger_info,false/* recomputeR*/,false /* don't compute res8*/,eliminate_flag,false /* 1 mod only */,parallel,interred,&gbasis_param.initsep,gbasis_param,coeffsptr?&gbasiscoeff:0)){
              if (debug_infolevel)
                *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" <<'\n';
	      get_newres_ckrur<tdeg_t14>(resmod,newres,v,G,env->modulo.val,rur,&gbasis_param.initsep,&gbasiscoeff,coeffsptr);
	      debug_infolevel=save_debuginfo; return true;
	    }
	  }
	  else {
	    if (in_gbasisf4buchbergermod<tdeg_t14>(res,resmod,G,env->modulo.val,true/*totaldeg*/,0,0,false)){
              if (debug_infolevel)
                *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" <<'\n';
	      get_newres_ckrur<tdeg_t14,modint>(resmod,newres,v,G,env->modulo.val,rur,&gbasis_param.initsep,0,0);
	      debug_infolevel=save_debuginfo; return true;
	    }
	  }
	} // end if gbasis computation modulo integer
	else {
#ifdef GIAC_REDUCEMODULO
	  vectpoly w(v);
	  reduce(w,env);
	  sort_vectpoly(w.begin(),w.end());
	  vectpoly_2_vectpoly8(w,order,res);
#endif
	  if (in_gbasis(res,G,env,true)){
            if (debug_infolevel)
              *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
	    get_newres(res,newres,v,G);
	    debug_infolevel=save_debuginfo; return true;
	  }
	}
      }  catch (std::runtime_error & e){ 
	last_evaled_argptr(contextptr)=NULL;
	CERR << "Degree too large for compressed monomials. Using uncompressed monomials instead." << '\n';
      }
    }
#endif
    if (v.front().dim<=11 && order.o==_REVLEX_ORDER){
      vectpoly8<tdeg_t11> res;
      vectpolymod<tdeg_t11,modint> resmod;
      vector<unsigned> G;
      vectpoly_2_vectpoly8<tdeg_t11>(v,order,res);
      // Temporary workaround until rur_compute support parametric rur
      if (rur && absint(order.o)!=-_RUR_REVLEX){ 
	rur=0;
	order.o=absint(order.o);
      }
      CLOCK_T c=CLOCK();
      if (modularalgo && (!env || env->modulo==0 || env->moduloon==false)){
        std::vector<giac::vectpoly8<tdeg_t11> > gbasis_coeffs;
	if (mod_gbasis<tdeg_t11,qmodint,qmodint2>(res,modularcheck,
		       //order.o==_REVLEX_ORDER /* zdata*/,
		       1 || !rur /* zdata*/,
		       rur,contextptr,gbasis_param,coeffsptr?&gbasis_coeffs:0)){
          if (debug_infolevel)
            *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
	  get_newres(res,newres,v,&gbasis_coeffs,coeffsptr);
	  debug_infolevel=save_debuginfo; return true;
	}
      }
      if (env && env->moduloon && env->modulo.type==_INT_){
	if (!res.empty() && (res.front().order.o==_REVLEX_ORDER || res.front().order.o==_3VAR_ORDER || res.front().order.o==_7VAR_ORDER || res.front().order.o==_11VAR_ORDER)){
	  vector<zinfo_t<tdeg_t11> > f4buchberger_info;
	  vector< vectpolymod<tdeg_t11,modint> > gbasiscoeff;
	  vector< paire > pairs_reducing_to_zero;
	  f4buchberger_info.reserve(GBASISF4_MAXITER);
	  if (zgbasis<tdeg_t11,modint,modint2>(res,resmod,G,env->modulo.val,true/*totaldeg*/,&pairs_reducing_to_zero,f4buchberger_info,false/* recomputeR*/,false /* don't compute res8*/,eliminate_flag,false /* 1 mod only */,parallel,interred,&gbasis_param.initsep,gbasis_param,coeffsptr?&gbasiscoeff:0)){
            if (debug_infolevel)
              *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
	    get_newres_ckrur<tdeg_t11>(resmod,newres,v,G,env->modulo.val,rur,&gbasis_param.initsep,&gbasiscoeff,coeffsptr);
	    debug_infolevel=save_debuginfo; return true;
	  }
	}
	else {
	  if (in_gbasisf4buchbergermod<tdeg_t11>(res,resmod,G,env->modulo.val,true/*totaldeg*/,0,0,false)){
            if (debug_infolevel)
              *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
	    get_newres_ckrur<tdeg_t11,modint>(resmod,newres,v,G,env->modulo.val,rur,&gbasis_param.initsep,0,0);
	    debug_infolevel=save_debuginfo; return true;
	  }
	}
      } // end if gbasis modulo integer
      else {
#ifdef GIAC_REDUCEMODULO
	vectpoly w(v);
	reduce(w,env);
	sort_vectpoly(w.begin(),w.end());
	vectpoly_2_vectpoly8(w,order,res);
#endif
	if (in_gbasis(res,G,env,true)){
          if (debug_infolevel)
            *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
	  get_newres(res,newres,v,G);
	  debug_infolevel=save_debuginfo; return true;
	}
      }
    }
    if (v.front().dim<=15 
	//&& order.o==_REVLEX_ORDER
	&&order.o<_16VAR_ORDER
	){
      vectpoly8<tdeg_t15> res;
      vectpolymod<tdeg_t15,modint> resmod;
      vector<unsigned> G;
      vectpoly_2_vectpoly8<tdeg_t15>(v,order,res);
      // Temporary workaround until rur_compute support parametric rur
      if (rur && absint(order.o)!=-_RUR_REVLEX){ 
	rur=0;
	order.o=absint(order.o);
      }
      CLOCK_T c=CLOCK();
      if (modularalgo && (!env || env->modulo==0 || env->moduloon==false)){
        std::vector<giac::vectpoly8<tdeg_t15> > gbasis_coeffs;
	if (mod_gbasis<tdeg_t15,qmodint,qmodint2>(res,modularcheck,
		       //order.o==_REVLEX_ORDER /* zdata*/,
		       1 || !rur /* zdata*/,
		       rur,contextptr,gbasis_param,coeffsptr?&gbasis_coeffs:0)){
          if (debug_infolevel)
            *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
	  get_newres(res,newres,v,&gbasis_coeffs,coeffsptr);
	  debug_infolevel=save_debuginfo; return true;
	}
      }
      if (env && env->moduloon && env->modulo.type==_INT_){
#ifdef GBASISF4_BUCHBERGER
	if (!res.empty() && (res.front().order.o==_REVLEX_ORDER || res.front().order.o==_3VAR_ORDER || res.front().order.o==_7VAR_ORDER || res.front().order.o==_11VAR_ORDER)){
	  vector<zinfo_t<tdeg_t15> > f4buchberger_info;
	  vector< vectpolymod<tdeg_t15,modint> > gbasiscoeff;
	  vector< paire > pairs_reducing_to_zero;
	  f4buchberger_info.reserve(GBASISF4_MAXITER);
	  if (!zgbasis<tdeg_t15,modint,modint2>(res,resmod,G,env->modulo.val,true/*totaldeg*/,&pairs_reducing_to_zero,f4buchberger_info,false/* recomputeR*/,false /* don't compute res8*/,eliminate_flag,false/* 1 mod only*/,parallel,interred,&gbasis_param.initsep,gbasis_param,coeffsptr?&gbasiscoeff:0))
	    return false;
          if (debug_infolevel)
            *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
#if 1
	  get_newres_ckrur<tdeg_t15>(resmod,newres,v,G,env->modulo.val,rur,&gbasis_param.initsep,&gbasiscoeff,coeffsptr);
#else
	  newres=vectpoly(G.size(),polynome(v.front().dim,v.front()));
	  for (unsigned i=0;i<G.size();++i)
	    resmod[G[i]].get_polynome(newres[i]);
#endif
	  debug_infolevel=save_debuginfo; return true;
	}
	else
	  in_gbasisf4buchbergermod<tdeg_t15>(res,resmod,G,env->modulo.val,true/*totaldeg*/,0,0,false);
#else
	in_gbasismod(res,resmod,G,env->modulo.val,true,0);
#endif
	if (debug_infolevel)
	  CERR << "G=" << G << '\n';
      }
      else { // env->modoloon etc.
#ifdef GIAC_REDUCEMODULO
	vectpoly w(v);
	reduce(w,env);
	sort_vectpoly(w.begin(),w.end());
	vectpoly_2_vectpoly8(w,order,res);
#endif
	in_gbasis(res,G,env,true);
      }
      newres=vectpoly(G.size(),polynome(v.front().dim,v.front()));
      for (unsigned i=0;i<G.size();++i)
	res[G[i]].get_polynome(newres[i]);
      debug_infolevel=save_debuginfo; return true;
    }
    vectpoly8<tdeg_t64> res;
    vectpolymod<tdeg_t64,modint> resmod;
    vector<unsigned> G;
    vectpoly_2_vectpoly8<tdeg_t64>(v,order,res);
    // Temporary workaround until rur_compute support parametric rur
    if (rur && absint(order.o)!=-_RUR_REVLEX){ 
      rur=0;
      order.o=absint(order.o);
    }
    CLOCK_T c=CLOCK();
    if (modularalgo && (!env || env->modulo==0 || env->moduloon==false)){
      std::vector<giac::vectpoly8<tdeg_t64> > gbasis_coeffs;
      if (mod_gbasis<tdeg_t64,qmodint,qmodint2>(res,modularcheck,
		     //order.o==_REVLEX_ORDER /* zdata*/,
		     1 || !rur /* zdata*/,
		     rur,contextptr,gbasis_param,coeffsptr?&gbasis_coeffs:0)){
        if (debug_infolevel)
          *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
        get_newres(res,newres,v,&gbasis_coeffs,coeffsptr);
	debug_infolevel=save_debuginfo; return true;
      }
    }
    if (env && env->moduloon && env->modulo.type==_INT_){
#ifdef GBASISF4_BUCHBERGER
      if (!res.empty() && (res.front().order.o==_REVLEX_ORDER || res.front().order.o==_3VAR_ORDER || res.front().order.o==_7VAR_ORDER || res.front().order.o==_11VAR_ORDER)){
	vector<zinfo_t<tdeg_t64> > f4buchberger_info;
	vector< vectpolymod<tdeg_t64,modint> > gbasiscoeff;
	vector< paire > pairs_reducing_to_zero;
	f4buchberger_info.reserve(GBASISF4_MAXITER);
	zgbasis<tdeg_t64,modint,modint2>(res,resmod,G,env->modulo.val,true/*totaldeg*/,&pairs_reducing_to_zero,f4buchberger_info,false/* recomputeR*/,false /* don't compute res8*/,eliminate_flag,false/* 1 mod only*/,parallel,interred,&gbasis_param.initsep,gbasis_param,coeffsptr?&gbasiscoeff:0);	
        if (debug_infolevel)
          *logptr(contextptr) << "// Groebner basis computation time " << (CLOCK()-c)*1e-6 <<  " Memory " << memory_usage()*1e-6 << "M" << '\n';
#if 1
	get_newres_ckrur<tdeg_t64>(resmod,newres,v,G,env->modulo.val,rur,&gbasis_param.initsep,&gbasiscoeff,coeffsptr);
#else
	newres=vectpoly(G.size(),polynome(v.front().dim,v.front()));
	for (unsigned i=0;i<G.size();++i)
	  resmod[G[i]].get_polynome(newres[i]);
#endif
	debug_infolevel=save_debuginfo; return true;
      }
      else
	in_gbasisf4buchbergermod<tdeg_t64>(res,resmod,G,env->modulo.val,true/*totaldeg*/,0,0,false);
#else
      in_gbasismod(res,resmod,G,env->modulo.val,true,0);
#endif
      if (debug_infolevel)
	CERR << "G=" << G << '\n';
    }
    else {
#ifdef GIAC_REDUCEMODULO
      vectpoly w(v);
      reduce(w,env);
      sort_vectpoly(w.begin(),w.end());
      vectpoly_2_vectpoly8(w,order,res);
#endif
      in_gbasis(res,G,env,true);
    }
    newres=vectpoly(G.size(),polynome(v.front().dim,v.front()));
    for (unsigned i=0;i<G.size();++i)
      res[G[i]].get_polynome(newres[i]);
    debug_infolevel=save_debuginfo; return true;
  }

  bool greduce8(const vectpoly & v,const vectpoly & gb_,order_t & order,vectpoly & newres,environment * env,GIAC_CONTEXT){
    if (v.empty()){ newres.clear(); return true;}
    vectpoly8<tdeg_t64> red,gb,quo;
    vectpoly_2_vectpoly8(v,order,red);
    vectpoly_2_vectpoly8(gb_,order,gb);
    poly8<tdeg_t64> rem,TMP1,TMP2;
    vector<unsigned> G; G_idn(G,gb_.size());
    int dim;
    for (int i=0;i<int(v.size());++i){
      quo.clear();
      rem.coord.clear();
      dim=red[i].dim;
      if (debug_infolevel>1)
	COUT << CLOCK()*1e-6 << " begin reduce poly no " << i << " #monomials " << red[i].coord.size() << '\n';
      gen lambda;
      reduce(red[i],gb,G,-1,quo,rem,TMP1,TMP2,lambda,env);
      if (debug_infolevel>1)
	COUT << CLOCK()*1e-6 << " end reduce poly no " << i << " #monomials " << rem.coord.size() << '\n';
      for (int j=0;j<int(rem.coord.size());++j){
	rem.coord[j].g=rem.coord[j].g/lambda;
      }
      red[i]=rem;
    }
    newres=vectpoly(v.size(),polynome(v.front().dim,v.front()));
    for (unsigned i=0;i<int(v.size());++i)
      red[i].get_polynome(newres[i]);
    return true;
  }

#ifdef GIAC_RHASH
  inline size_t tdeg_t64_hash_function_object::operator () (const tdeg_t64 & v) const
  { return v.hash; }
#endif
  
#endif // CAS38_DISABLED

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC

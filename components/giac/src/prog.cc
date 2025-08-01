/* -*- mode:C++ ; compile-command: "g++ -DHAVE_CONFIG_H -I. -I.. -DIN_GIAC -DGIAC_GENERIC_CONSTANTS  -g -c -fno-strict-aliasing prog.cc -Wall" -*- */
#include "giacPCH.h"

/*
 *  Copyright (C) 2001,14 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
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
#ifdef __MINGW_H
#define HAVE_NO_PWD_H
#endif

#ifndef HAVE_NO_PWD_H
#ifndef BESTA_OS
#include <pwd.h>
#endif
#endif
#ifdef FXCG
extern "C" {
#include <rtc.h>
}
#endif
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#if !defined GIAC_HAS_STO_38 && !defined NSPIRE && !defined FXCG
#include <fstream>
#endif
#include "prog.h"
#include "identificateur.h"
#include "symbolic.h"
#include "identificateur.h"
#include "usual.h"
#include "sym2poly.h"
#include "subst.h"
#include "plot.h"
#include "tex.h"
#include "input_parser.h"
#include "input_lexer.h"
#include "rpn.h"
#include "help.h"
#include "ti89.h" // for _unarchive_ti
#include "permu.h"
#include "modpoly.h"
#include "unary.h"
#include "input_lexer.h"
#include "maple.h"
#include "derive.h"
#include "giacintl.h"
#include "misc.h"
#include "lin.h"
#include "pari.h"
#include "intg.h"
#include "csturm.h"
#include "sparse.h"
#include "quater.h"
#include "giacintl.h"
#ifndef GIAC_GGB
#ifdef KHICAS
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_final(SHA256_CTX *ctx, BYTE hash[]);
#else
#ifndef EMCC2
#include "sha256.h"
#endif
#endif
#endif
#if defined GIAC_HAS_STO_38 || defined NSPIRE || defined NSPIRE_NEWLIB || defined FXCG || defined GIAC_GGB || defined USE_GMP_REPLACEMENTS || defined KHICAS || defined SDL_KHICAS
#else
#include "signalprocessing.h"
#endif
// #include "input_parser.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#endif // HAVE_LIBDL
#ifdef USE_GMP_REPLACEMENTS
#undef HAVE_GMPXX_H
#undef HAVE_LIBMPFR
#undef HAVE_LIBPARI
#endif

#ifdef NSPIRE_NEWLIB
#include <os.h>
extern "C" double millis();
#endif

//#ifdef BESTA_OS
//unsigned int PrimeGetNow(); 
//#endif

#ifdef RTOS_THREADX 
u32 PrimeGetNow();
extern "C" uint32_t mainThreadStack[];
#else
#if !defined BESTA_OS && !defined FXCG
#include <time.h>
#endif
#endif

#if defined(EMCC) || defined(EMCC2)
#include <emscripten.h>
int emfltkdbg=0; // set to 1 to enable emscripten_loop inside debugger
// this is much more comfortable, but will only work if called from
// Xcas main menu, it does not work if called from a commandline
#endif

#if defined KHICAS || defined SDL_KHICAS
#include "kdisplay.h"
#endif

#if defined EMCC2 && defined HAVE_LIBFLTK
#include <FL/Fl_Group.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Tooltip.H>
#include "hist.h"
#include "Xcas1.h"
Fl_Group * emdbg_w=0;
#endif

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

#if 1 // defined(GIAC_HAS_STO_38) && defined(VISUALC)
  const int rand_max2=2147483647;
#else
  const int rand_max2=RAND_MAX;
#endif
  const double inv_rand_max2_p1=1.0/(rand_max2+1.0);

#ifdef HAVE_LIBDL
  modules_tab giac_modules_tab;
#endif

  void alert(const string & s,GIAC_CONTEXT){
#if defined(EMCC) || defined(EMCC2)
    EM_ASM_ARGS({
	if (typeof UI!=='undefined' && UI.warnpy){
          var msg = UTF8ToString($0);// Pointer_stringify($0); // Convert message to JS string
          alert(msg);                      // Use JS version of alert          
        }
      }, s.c_str());
#endif
    *logptr(contextptr) << s << '\n';
  }

  gen equaltosto(const gen & g,GIAC_CONTEXT){
    if (g.type<=_IDNT || !eval_equaltosto(contextptr))
      return g;
    if (g.is_symb_of_sommet(at_add_autosimplify)){
      return symbolic(g._SYMBptr->sommet,equaltosto(g._SYMBptr->feuille,contextptr));
    }
    if (is_equal(g)){
      vecteur v=*g._SYMBptr->feuille._VECTptr;
      gen a;
      if (v.size()==2)
	a=v.back();
      else
	a=gen(vecteur(v.begin()+1,v.end()),g.subtype);
      if (v.front().type==_IDNT)
	return symb_sto(a,v.front());
      if (v.front().type==_VECT){
	vecteur w=*v.front()._VECTptr;
	for (int i=0;i<w.size();++i){
	  if (w[i].type!=_IDNT)
	    return g;
	}
	return symb_sto(a,v.front());
      }
    }
    return g;
  }

  vecteur equaltostov(const vecteur & v,GIAC_CONTEXT){
    vecteur w(v);
    iterateur it=w.begin(),itend=w.end();
    for (;it!=itend;++it)
      *it=equaltosto(*it,contextptr);
    return w;
  }

  gen attoof(const gen & g){
    if (g.type==_VECT){
      vecteur v=*g._VECTptr;
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it)
       *it=attoof(*it);
      return gen(v,g.subtype);
    }
    if (g.type!=_SYMB)
      return g;
    if (g._SYMBptr->sommet!=at_at)
      return symbolic(g._SYMBptr->sommet,attoof(g._SYMBptr->feuille));
    if (g._SYMBptr->feuille.type!=_VECT || g._SYMBptr->feuille._VECTptr->size()<=1) // not somehting that I recognize as a proper parameter list, just return as before
      return symbolic(at_of,attoof(g._SYMBptr->feuille));
    // This looks like it is a vector of at least 2 objects. The first one should be the variable to do an at/of on and all the rest should be indices that needs to be incremented
    vecteur v=*g._SYMBptr->feuille._VECTptr; 
    iterateur it=v.begin()+1,itend=v.end(); // from item 1 to n-1 in the vector
    for (;it!=itend;++it) 
      *it=attoof(*it)+gen(1); // add 1 to the object
    return symbolic(at_of,attoof(gen(v,g._SYMBptr->feuille.subtype))); // return the of(vector with modified indices) gen
  }

  static int prog_eval_level(GIAC_CONTEXT){
    if (int i=prog_eval_level_val(contextptr))
      return i;
    return std::max(1,eval_level(contextptr));
  }

  gen check_secure(){
    if (secure_run
#ifdef KHICAS
	|| exam_mode || nspire_exam_mode
#endif
	)
      return gensizeerr(gettext("Running in secure mode"));
    return 0;
  }

  string indent(GIAC_CONTEXT){
    if (xcas_mode(contextptr)==3)
      return "\n:"+string(debug_ptr(contextptr)->indent_spaces,' ');
    else
      return " \n"+string(debug_ptr(contextptr)->indent_spaces,' ');
  }

  static string indent2(GIAC_CONTEXT){
    return string(debug_ptr(contextptr)->indent_spaces,' ');
  }

  // static gen substsametoequal(const gen & g){  return symbolic(at_subst,apply(g,sametoequal)); }

  // static gen subssametoequal(const gen & g){  return symbolic(at_subs,apply(g,sametoequal));  }

  // static gen maplesubssametoequal(const gen & g){  return symbolic(at_maple_subs,apply(g,sametoequal)); }

  gen equaltosame(const gen & a){
    // full replacement of = by == has been commented to avoid
    // problems with tests like: if (limit(...,x=0,..))
    /*
    unary_function_ptr equaltosametab1[]={at_equal,at_subst,at_subs,at_maple_subs};
    vector<unary_function_ptr> substin(equaltosametab1,equaltosametab1+sizeof(equaltosametab1)/sizeof(unary_function_ptr));
    gen_op equaltosametab2[]={symb_same,substsametoequal,subssametoequal,maplesubssametoequal};
    vector<gen_op> substout(equaltosametab2,equaltosametab2+sizeof(equaltosametab2)/sizeof(gen_op));
    gen tmp=subst(a,substin,substout,true);
    return tmp;
    */
    if ( is_equal(a))
      return symb_same(a._SYMBptr->feuille._VECTptr->front(),a._SYMBptr->feuille._VECTptr->back());
    else
      return a;
  }

  gen sametoequal(const gen & a){
    if ( (a.type==_SYMB) && (a._SYMBptr->sommet==at_same) )
      return symb_equal(a._SYMBptr->feuille._VECTptr->front(),a._SYMBptr->feuille._VECTptr->back());
    else
      return a;
  }

  static void increment_instruction(const const_iterateur & it0,const const_iterateur & itend,debug_struct * dbgptr){
    const_iterateur it=it0;
    for (;it!=itend;++it)
      increment_instruction(*it,dbgptr);
  }

  void increment_instruction(const vecteur & v,debug_struct * dbgptr){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      increment_instruction(*it,dbgptr);
  }

  void increment_instruction(const gen & arg,debug_struct * dbgptr){
    // cerr << debug_ptr(contextptr)->current_instruction << " " << arg <<'\n';
    ++dbgptr->current_instruction;
    if (arg.type!=_SYMB)
      return;
    const unary_function_ptr & u=arg._SYMBptr->sommet;
    const gen & f=arg._SYMBptr->feuille;
    const unary_function_eval * uptr=dynamic_cast<const unary_function_eval *>(u.ptr());
    if (uptr && uptr->op==_ifte){
      --dbgptr->current_instruction;
      increment_instruction(*f._VECTptr,dbgptr);
      return;
    }
    if ( (u==at_local) || (uptr && uptr->op==_for) ){
      gen F(f._VECTptr->back());
      if (F.type!=_VECT){
	if (F.is_symb_of_sommet(at_bloc) && F._SYMBptr->feuille.type==_VECT)
	  increment_instruction(*F._SYMBptr->feuille._VECTptr,dbgptr);
	else
	  increment_instruction(F,dbgptr);
      }
      else 
	increment_instruction(*F._VECTptr,dbgptr);
      return;
    }
    if (u==at_bloc){
      if (f.type!=_VECT)
	increment_instruction(f,dbgptr);
      else
	increment_instruction(*f._VECTptr,dbgptr);
      return;
    }
    if (u==at_try_catch){
      increment_instruction(f._VECTptr->front(),dbgptr);
      increment_instruction(f._VECTptr->back(),dbgptr);
    }
  }

  void increment_instruction(const gen & arg,GIAC_CONTEXT){
    increment_instruction(arg,debug_ptr(contextptr));
  }

  static string concatenate(const vector<string> & v){
    vector<string>::const_iterator it=v.begin(),itend=v.end();
    string res;
    for (;it!=itend;++it){
      res=res + "  "+*it;
    }
    return res;
  }

  void debug_print(const vecteur & arg,vector<string> & v,GIAC_CONTEXT){
    const_iterateur it=arg.begin(),itend=arg.end();
    for (;it!=itend;++it){
      if (it->is_symb_of_sommet(at_program)){
	vector<string> tmp;
	debug_print(*it,tmp,contextptr);
	v.push_back(concatenate(tmp));
      }
      debug_print(*it,v,contextptr);
    }
  }

  static string printaslocalvars(const gen &loc,GIAC_CONTEXT){
    gen locals(loc);
    if (locals._VECTptr->size()==1)
      return locals._VECTptr->front().print(contextptr);
    locals.subtype=_SEQ__VECT;
    return locals.print(contextptr);
  }

  // convert back increment and decrement to sto
  static gen from_increment(const gen & g){
    int type=0;
    if (g.is_symb_of_sommet(at_increment))
      type=1;
    if (g.is_symb_of_sommet(at_decrement))
      type=-1;
    if (type){
      gen & f =g._SYMBptr->feuille;
      if (f.type!=_VECT)
	return symbolic(at_sto,gen(makevecteur(symbolic(at_plus,makevecteur(f,type)),f),_SEQ__VECT));
      vecteur & v = *f._VECTptr;
      if (v.size()!=2)
	return gensizeerr(gettext("from_increment"));
      return symbolic(at_sto,gen(makevecteur(symbolic(at_plus,gen(makevecteur(v[0],type*v[1]),_SEQ__VECT)),v[0]),_SEQ__VECT));
    }
    return g;
  }

  void debug_print(const gen & e,vector<string>  & v,GIAC_CONTEXT){
    if (e.type!=_SYMB){
      v.push_back(indent2(contextptr)+e.print(contextptr));
      return ;
    }
    bool is38=abs_calc_mode(contextptr)==38;
    bool python=python_compat(contextptr);
    unary_function_ptr u=e._SYMBptr->sommet;
    gen f=e._SYMBptr->feuille;
    const unary_function_eval * uptr=dynamic_cast<const unary_function_eval *>(u.ptr());
    if (uptr && uptr->op==_ifte){
      string s=indent2(contextptr);
      s += python?"if ":(is38?"IF ":"if(");
      vecteur w=*f._VECTptr;
      s += w.front().print(contextptr);
      s += python?":":(is38?" THEN/ELSE":")");
      v.push_back(s);
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(w[1],v,contextptr);
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(w[2],v,contextptr);
      debug_ptr(contextptr)->indent_spaces -=2;
      return ;
    }
    if (u==at_local){
      string s(indent2(contextptr));
      s += python?"# local ":(is38?"LOCAL ":"local ");
      gen local_global=f._VECTptr->front(),locals(gen2vecteur(local_global)),globals(vecteur(0));
      if (local_global.type==_VECT && local_global._VECTptr->size()==2){ 
	gen f=local_global._VECTptr->front(),b=local_global._VECTptr->back();
	if (f.type!=_IDNT){
	  locals=gen2vecteur(f);
	  globals=gen2vecteur(b);
	}
      }
      if (globals._VECTptr->empty())
	s += printaslocalvars(locals,contextptr);
      else
	s += local_global.print(contextptr);
      v.push_back(s);
      debug_ptr(contextptr)->indent_spaces += 2;
      f=f._VECTptr->back();
      if (f.type!=_VECT)
	debug_print(f,v,contextptr);
      else
	debug_print(*f._VECTptr,v,contextptr);
      debug_ptr(contextptr)->indent_spaces -= 2;
      return;
    }
    if (uptr && uptr->op==_for){
      vecteur w=*f._VECTptr;
      string s(indent2(contextptr));
      bool done=false;
      if (python){
	gen inc(from_increment(w[2]));
	if (inc.is_symb_of_sommet(at_sto) && inc._SYMBptr->feuille[1].type==_IDNT){
	  gen index=inc._SYMBptr->feuille[1];
	  if (inc._SYMBptr->feuille[0]==index+1 && w[0].is_symb_of_sommet(at_sto) && w[0]._SYMBptr->feuille[1]==index){
	    gen start=w[0]._SYMBptr->feuille[0];
	    if (w[1].type==_SYMB && (w[1]._SYMBptr->sommet==at_inferieur_egal || w[1]._SYMBptr->sommet==at_inferieur_strict) && w[1]._SYMBptr->feuille[0]==index){
	      int large=w[1]._SYMBptr->sommet==at_inferieur_egal?1:0;
	      gen stop=w[1]._SYMBptr->feuille[1];
	      s += "for "+index.print(contextptr)+" in range(";
	      if (start!=0)
		s +=start.print(contextptr)+",";
	      s += (stop+large).print(contextptr)+"):";
	      done=true;
	    }
	  }
	}
      }
      if (w[0].type==_INT_ && w[2].type==_INT_){
	s += python?"while ":(is38?"WHILE(":"while(");
	s += w[1].print(contextptr);
	s += python?":":")";
	done=true;
      }
      if (!done){
	s += is38?"FOR(":"for(";
	s += w[0].print(contextptr)+";"+w[1].print(contextptr)+";"+w[2].print(contextptr)+")";
      }
      v.push_back(s);
      debug_ptr(contextptr)->indent_spaces += 2;
      f=f._VECTptr->back();
      if ((f.type==_SYMB) && (f._SYMBptr->sommet==at_bloc))
	f=f._SYMBptr->feuille;
      if (f.type!=_VECT)
	debug_print(f,v,contextptr);
      else
	debug_print(*f._VECTptr,v,contextptr);
      debug_ptr(contextptr)->indent_spaces -= 2;
      return;
    }
    if (u==at_bloc){
      v.push_back(string(indent2(contextptr)+"bloc"));
      debug_ptr(contextptr)->indent_spaces += 2;
      if (f.type!=_VECT)
	debug_print(f,v,contextptr);
      else
	debug_print(*f._VECTptr,v,contextptr);
      debug_ptr(contextptr)->indent_spaces -= 2;
      return;
    }
    if (u==at_try_catch){
      // cerr << f << '\n';
      v.push_back(string(indent2(contextptr)+"try"));
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(f._VECTptr->front(),v,contextptr);
      debug_ptr(contextptr)->indent_spaces += 1;
      debug_print(f._VECTptr->back(),v,contextptr);
      debug_ptr(contextptr)->indent_spaces -=2;
      return;
    }
    v.push_back(indent2(contextptr)+e.print(contextptr));
  }

  static vecteur rm_checktype(const vecteur & v,GIAC_CONTEXT){
    vecteur addvars(v);
    iterateur it=addvars.begin(),itend=addvars.end();
    for (;it!=itend;++it){
      if (it->is_symb_of_sommet(at_deuxpoints))
	*it=it->_SYMBptr->feuille._VECTptr->front();	
      if (it->is_symb_of_sommet(at_check_type))
	*it=it->_SYMBptr->feuille._VECTptr->back();
      if (it->is_symb_of_sommet(at_sto))
	*it=it->_SYMBptr->feuille._VECTptr->back();	
      if (it->is_symb_of_sommet(at_equal))
	*it=it->_SYMBptr->feuille._VECTptr->front();	
    }
    return addvars;
  }
  // res1= list of assignation with =, res2= list of non declared global vars, res3= list of declared global vars, res4=list of functions
  void check_local_assign(const gen & g,const vecteur & prog_args,vecteur & res1,vecteur & res2,vecteur & res3,vecteur & res4,bool testequal,GIAC_CONTEXT){
    if (g.is_symb_of_sommet(at_double_deux_points))
      return;
    if (g.is_symb_of_sommet(at_local)){
      gen &f=g._SYMBptr->feuille;
      if (f.type!=_VECT || f._VECTptr->size()!=2)
	return;
      gen declaredvars=f._VECTptr->front();
      if (declaredvars.type==_VECT && declaredvars._VECTptr->size()==2){
	vecteur f1(gen2vecteur(declaredvars._VECTptr->front()));
	for (int i=0;i<int(f1.size());++i){
	  if (f1[i].is_symb_of_sommet(at_equal))
	    f1[i]=f1[i]._SYMBptr->feuille[0];
	}
	vecteur f2(gen2vecteur(declaredvars._VECTptr->back()));
	res3=mergevecteur(res3,f2);
	declaredvars=mergevecteur(f1,f2);
      }
      vecteur addvars(rm_checktype(gen2vecteur(declaredvars),contextptr));
      vecteur newvars(mergevecteur(prog_args,addvars));
      // check_local_assign(f._VECTptr->front(),newvars,res1,res2,res3,res4,testequal,contextptr);
      check_local_assign(f._VECTptr->back(),newvars,res1,res2,res3,res4,testequal,contextptr);
      return; 
    }
    if (g.is_symb_of_sommet(at_sto)){
      gen &f=g._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front().is_symb_of_sommet(at_program)){
	res4.push_back(f._VECTptr->back());
	gen & ff = f._VECTptr->front()._SYMBptr->feuille;
	if (ff.type==_VECT && ff._VECTptr->size()==3){
	  vecteur alt_prog_args(gen2vecteur(ff._VECTptr->front()));
	  check_local_assign(ff._VECTptr->back(),alt_prog_args,res1,res2,res3,res4,testequal,contextptr);
	}
	return;
      }
    }
    if (g.is_symb_of_sommet(at_ifte) || g.is_symb_of_sommet(at_when)){
      vecteur v=lop(g,at_array_sto);
      if (!v.empty() && logptr(contextptr))
	*logptr(contextptr) << gettext("Warning, =< is in-place assign, check ") << v << '\n';
    }
    if (g.is_symb_of_sommet(at_bloc) || 
	g.is_symb_of_sommet(at_for) || g.is_symb_of_sommet(at_pour) ||
	g.is_symb_of_sommet(at_tantque) || g.is_symb_of_sommet(at_si) ||
	g.is_symb_of_sommet(at_sialorssinon) || 
	g.is_symb_of_sommet(at_ifte) || g.is_symb_of_sommet(at_when)){
      check_local_assign(g._SYMBptr->feuille,prog_args,res1,res2,res3,res4,testequal,contextptr);
      return;
    }
    if (testequal && is_equal(g)){
      if (g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->front().type!=_INT_ )
	res1.push_back(g);
      return;
    }
    if (g.is_symb_of_sommet(at_of)){
      gen & f=g._SYMBptr->feuille;
      if (f.type!=_VECT || f._VECTptr->size()!=2)
	return;
      if (python_compat(contextptr) || protecteval(f._VECTptr->front(),1,contextptr)!=f._VECTptr->front())
	check_local_assign(f._VECTptr->back(),prog_args,res1,res2,res3,res4,false,contextptr);
      else
	check_local_assign(f,prog_args,res1,res2,res3,res4,false,contextptr);
      return;
    }
    if (g.type==_SYMB){
      check_local_assign(g._SYMBptr->feuille,prog_args,res1,res2,res3,res4,false,contextptr);
      return;
    }
    if (g.type!=_VECT){
      vecteur l(gen2vecteur(_lname(g,contextptr)));
      const_iterateur it=l.begin(),itend=l.end();
      for (;it!=itend;++it){
	if (!equalposcomp(res2,*it) && !equalposcomp(prog_args,*it))
	  res2.push_back(*it);
      }
      return;
    }
    const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
    for (;it!=itend;++it){
      check_local_assign(*it,prog_args,res1,res2,res3,res4,testequal,contextptr);
    }
  }
  bool is_constant_idnt(const gen & g){
    return g==cst_pi || g==cst_euler_gamma || is_inf(g) || is_undef(g) || (g.type==_IDNT && (strcmp(g._IDNTptr->id_name,"i")==0 || strcmp(g._IDNTptr->id_name,"None")==0 || strcmp(g._IDNTptr->id_name,"cmath")==0 || strcmp(g._IDNTptr->id_name,"math")==0 || strcmp(g._IDNTptr->id_name,"kandinsky")==0 || strcmp(g._IDNTptr->id_name,"pass")==0));
  }

  bool warn_equal_in_prog=true,warn_symb_program_sto=true;
  gen _warn_equal_in_prog(const gen & g,GIAC_CONTEXT){
    if (is_zero(g) && g.type!=_VECT){
      warn_equal_in_prog=false;
      return string2gen(gettext("Warning disabled"),false);
    }
    if (is_one(g)){
      warn_equal_in_prog=true;
      return string2gen(gettext("Warning enabled"),false);
    }
    return warn_equal_in_prog;
  }
  static const char _warn_equal_in_prog_s []="warn_equal_in_prog";
  static define_unary_function_eval (__warn_equal_in_prog,&_warn_equal_in_prog,_warn_equal_in_prog_s);
  define_unary_function_ptr5( at_warn_equal_in_prog ,alias_at_warn_equal_in_prog,&__warn_equal_in_prog,0,true);

  // Return the names of variables that are not local in g
  // and the equality that are not used (warning = instead of := )
  string check_local_assign(const gen & g,GIAC_CONTEXT){
    string res;
    if (g.type==_VECT){
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it)
	res += check_local_assign(*it,contextptr);
      return res;
    }
    if (g.is_symb_of_sommet(at_nodisp))
      return check_local_assign(g._SYMBptr->feuille,contextptr);
    if (g.is_symb_of_sommet(at_sto)){
      gen & f =g._SYMBptr->feuille;
      if (f.type!=_VECT || f._VECTptr->size()!=2)
	return res;
      res=check_local_assign(f._VECTptr->front(),contextptr);
      return res.substr(0,res.size()-1)+"\n//"+gettext(" compiling ")+f._VECTptr->back().print(contextptr)+'\n';
    }
    if (!g.is_symb_of_sommet(at_program))
      return res;
    gen & f=g._SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=3)
      return gettext("// Invalid program");
    vecteur & v =*f._VECTptr;
    vecteur vars=gen2vecteur(v[0]),res1,res2,res3,res4;
    // add implicit declaration of global var in argument optional value
    for (int i=0;i<vars.size();i++){
      if (vars[i].is_symb_of_sommet(at_equal)){
	gen g=vars[i]._SYMBptr->feuille[1];
	res2=mergevecteur(res2,lidnt(g));
      }
    }
    res2.push_back(undef);
    vars=rm_checktype(vars,contextptr);
    for (unsigned i=0;i<vars.size();++i){
      if (equalposcomp(vars,vars[i])!=int(i+1))
	res += gettext("// Warning, duplicate argument name: ")+vars[i].print(contextptr)+'\n';
    }
    gen prog=v.back();
    check_local_assign(prog,vars,res1,res2,res3,res4,true,contextptr);
    int rs=int(res2.size());
    for (int i=0;i<rs;i++){
      if (is_constant_idnt(res2[i])){
	res2.erase(res2.begin()+i);
	--i; --rs;
      }
    }
    if (warn_equal_in_prog && !res1.empty()){ // syntax = for := is now accepted
      res += gettext("// Warning, assignation is :=, check the lines below:\n");
      res += "// (Run warn_equal_in_prog(0) to disable this warning)\n";
      const_iterateur it=res1.begin(),itend=res1.end();
      for (;it!=itend;++it){
	res += '\n'+it->print(contextptr);
      }
      res +="\n";
    }
    if (res2.size()>=1){
      res+=gettext("// Warning: ");
      const_iterateur it=res2.begin(),itend=res2.end();
      for (;it!=itend;++it){
	// pi already checked if (*it!=cst_pi)
	res += it->print(contextptr)+",";
      }
      res +=gettext(" declared as global variable(s). If symbolic variables are required, declare them as local and run purge\n");
    }
    if (res.empty())
      return first_error_line(contextptr)?gettext("// Error(s)\n"):gettext("// Success\n");
    else
      return res;
  }

  static string printascheck_type(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    return print_the_type(feuille._VECTptr->front().val,contextptr)+' '+feuille._VECTptr->back().print(contextptr);
  }
  
  gen symb_check_type(const gen & args,GIAC_CONTEXT){
    return symbolic(at_check_type,args);
  }
  gen _check_type(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_check_type(args,contextptr);
    if (args._VECTptr->size()!=2)
      return gensizeerr(gettext("check_type must have 2 args"));
    gen res=args._VECTptr->back();
    gen req=args._VECTptr->front();
    if (req.type!=_INT_) // FIXME check for matrix(...) or vector(...)
      return res;
    int type;
    switch (res.type){
    case _INT_:
      type=_ZINT;
      break;
    case _REAL:
      type=_DOUBLE_;
      break;
    default:
      type=res.type;
      break;
    }   
    if (req.val==_MAPLE_LIST){
      if (type==_VECT)
	return res;
      return gensizeerr(contextptr);
    }
    if (type==req.val)
      return res;
    if (type==_ZINT && type==(req.val &0xff) ){
      if (req.val==_POSINT && is_strictly_positive(res,contextptr)) 
	return res;
      if (req.val==_NEGINT && is_strictly_positive(-res,contextptr))
	return res;
      if (req.val==_NONPOSINT && is_positive(-res,contextptr))
	return res;
      if (req.val==_NONNEGINT && is_positive(res,contextptr))
	return res;
    }
    return gentypeerr(gettext("Argument should be of type ")+print_the_type(args._VECTptr->front().val,contextptr));
  }
  static const char _check_type_s []="check_type";
  static define_unary_function_eval2_index (118,__check_type,&symb_check_type,_check_type_s,&printascheck_type);
  define_unary_function_ptr( at_check_type ,alias_at_check_type ,&__check_type);

  // static gen symb_type(const gen & args){  return symbolic(at_type,args); }
  gen _type(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    int type;
    switch (args.type){
    case _INT_:
      type=_ZINT;
      break;
    case _REAL: 
#ifndef NO_RTTI
      if (dynamic_cast<real_interval *>(args._REALptr))
	type=_FLOAT_;
      else
#endif      
	type=_DOUBLE_; 
      break;
    default:
      if (args.is_symb_of_sommet(at_program))
	type=_FUNC;
      else
	type=args.type;
    }   
    gen tmp(type);
    tmp.subtype=1;
    return tmp;
  }
  static const char _type_s []="type";
  static define_unary_function_eval (__type,&_type,_type_s);
  define_unary_function_ptr5( at_type ,alias_at_type,&__type,0,true);

  gen _nop(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG &&  a.subtype==-1) return  a;
    if (a.type==_VECT && a.subtype==_SEQ__VECT){
      // Workaround so that sequences inside spreadsheet are saved as []
      gen tmp=a;
      tmp.subtype=0;
      return tmp;
    }
    return a;
  }
  static const char _nop_s []="nop";
  static define_unary_function_eval (__nop,&_nop,_nop_s);
  define_unary_function_ptr5( at_nop ,alias_at_nop,&__nop,0,true);

  gen _Nop(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG &&  a.subtype==-1) return  a;
    return a;
  }
  static const char _Nop_s []="Nop";
  static define_unary_function_eval (__Nop,&_Nop,_Nop_s);
  define_unary_function_ptr5( at_Nop ,alias_at_Nop,&__Nop,0,true);

  string printasinnerbloc(const gen & feuille,GIAC_CONTEXT){
    if ( (feuille.type==_SYMB) && feuille._SYMBptr->sommet==at_bloc)
      return printasinnerbloc(feuille._SYMBptr->feuille,contextptr);
    if (feuille.type!=_VECT)
      return indent(contextptr)+feuille.print(contextptr);
    const_iterateur it=feuille._VECTptr->begin(),itend=feuille._VECTptr->end();
    string res;
    if (it==itend)
      return res;
    for (;;){
      res += indent(contextptr)+it->print(contextptr);
      ++it;
      if (it==itend)
	return res;
      if (xcas_mode(contextptr)!=3)
	res += ";";
    }
  }

  static void local_init(const gen & e,vecteur & non_init_vars,vecteur & initialisation_seq){
    vecteur v;
    if (e.type!=_VECT)
      v=vecteur(1,e);
    else
      v=*e._VECTptr;
    if (v.size()==2 && v.front().type==_VECT)
      v=*v.front()._VECTptr;
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (it->type==_IDNT){
	non_init_vars.push_back(*it);
	continue;
      }
      if ( (it->type==_SYMB) && (it->_SYMBptr->sommet==at_sto)){
	non_init_vars.push_back(it->_SYMBptr->feuille._VECTptr->back());
	initialisation_seq.push_back(*it);
      }
    }
  }

  static gen add_global(const gen & i,GIAC_CONTEXT){
#ifndef NO_STDEXCEPT
    if (i.type==_IDNT)
#endif
      return identificateur("global_"+i.print(contextptr));
    return gensizeerr(gettext("Proc Parameters"));
  }

  static string remove_empty_lines(const string & s){
    // return s;
    string res;
    int ss=int(s.size()),ns=0;
    bool blank=true;
    for (int i=0;i<ss;++i){
      char ch=s[i];
      if (!blank){
	res += ch;
	if (ch=='\n'){
	  ns=0;
	  blank=true;
	}
	continue;
      }
      if (ch=='\n'){
	ns=0;
	continue;
      }
      if (ch!=' '){
	blank=false;
	res += string(ns,' ')+ch;
	ns=0;
	continue;
      }
      ++ns;
    }
    return res;
  }

  static string printasbloc(const gen & feuille,const char * sommetstr,GIAC_CONTEXT);
  
  static string in_printasprogram(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=3) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    string res;
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      vecteur v =*feuille._VECTptr;
      if (v[2].type==_VECT && v[2].subtype==_PRG__VECT)
	v[2]=symb_bloc(v[2]);
      if (!v[2].is_symb_of_sommet(at_bloc) && !v[2].is_symb_of_sommet(at_local)){
	res=string(ind,' ')+"lambda ";
	if (v[0].type==_VECT && v[0].subtype==_SEQ__VECT && v[0]._VECTptr->size()==1)
	  res += v[0]._VECTptr->front().print(contextptr);
	else 
	  res += v[0].print(contextptr);
	res +=':';
	if (v[2].is_symb_of_sommet(at_return))
	  res += v[2]._SYMBptr->feuille.print(contextptr);
	else
	  res += v[2].print(contextptr);
	return res;
      }
      res = string(ind,' ')+"def "+lastprog_name(contextptr)+"(";
      if (v[0].type==_VECT && v[0].subtype==_SEQ__VECT && v[0]._VECTptr->size()==1)
	res += equaltosto(v[0]._VECTptr->front(),contextptr).print(contextptr);
      else
	res += equaltosto(v[0],contextptr).print(contextptr);
      res += "):\n";
      ind += 4;
      if (v[2].is_symb_of_sommet(at_bloc) || v[2].is_symb_of_sommet(at_local))
	res += v[2].print(contextptr);
      else
	res += string(ind,' ')+v[2].print(contextptr)+'\n';
      ind -= 4;
      // remove empty lines in res
      return remove_empty_lines(res);
    }
    bool calc38=abs_calc_mode(contextptr)==38;
    if (!calc38){
      if (xcas_mode(contextptr)==3)
	res="\n:lastprog";
      else
	res=" "; // was res=indent(contextptr);
    }
    gen & feuille0=feuille._VECTptr->front();
    if (feuille0.type==_VECT && feuille0.subtype==_SEQ__VECT && feuille0._VECTptr->size()==1)
      res +="("+feuille0._VECTptr->front().print(contextptr)+")";
    else
      res +="("+feuille0.print(contextptr)+")";
    if (xcas_mode(contextptr)==3)
      res +="\n";
    else
      res += "->";
    bool test;
    string locals,inits;
    gen proc_args=feuille._VECTptr->front();
    vecteur vect,non_init_vars,initialisation_seq;
    if ((xcas_mode(contextptr)>0 || abs_calc_mode(contextptr)==38) && (feuille._VECTptr->back().type==_SYMB) && (feuille._VECTptr->back()._SYMBptr->sommet==at_local)){
      test=false;
      gen tmp=feuille._VECTptr->back()._SYMBptr->feuille;
      local_init(tmp._VECTptr->front(),non_init_vars,initialisation_seq);
      // For Maple add proc parameters to local vars
      if (xcas_mode(contextptr) ==1+_DECALAGE){
	if (proc_args.type==_VECT){
	  vecteur v=*proc_args._VECTptr;
	  non_init_vars=mergevecteur(non_init_vars,v);
	  iterateur it=v.begin(),itend=v.end();
	  for (;it!=itend;++it){
	    gen tmp=add_global(*it,contextptr);
	    initialisation_seq.push_back(symb_sto(tmp,*it));
	    *it=tmp;
	  }
	  proc_args=gen(v,_SEQ__VECT);
	}
	else {
	  non_init_vars.push_back(proc_args);
	  gen tmp=add_global(proc_args,contextptr);
	  initialisation_seq.push_back(symb_sto(tmp,proc_args));
	  proc_args=tmp;
	}
      }
      if (!non_init_vars.empty()){
	if (xcas_mode(contextptr)==3)
	  locals=indent(contextptr)+"Local "+printinner_VECT(non_init_vars,_SEQ__VECT,contextptr);
	else
	  locals=indent(contextptr)+(calc38?" LOCAL ":"  local ")+printinner_VECT(non_init_vars,_SEQ__VECT,contextptr)+";";
      }
      inits=printasinnerbloc(gen(initialisation_seq,_SEQ__VECT),contextptr);
      if (tmp._VECTptr->back().type==_VECT)
	vect=*tmp._VECTptr->back()._VECTptr;
      else
	vect=makevecteur(tmp._VECTptr->back());
    }
    else {
      test=(feuille._VECTptr->back().type!=_VECT ||feuille._VECTptr->back().subtype );
      if (!test)
	vect=*feuille._VECTptr->back()._VECTptr;
    }
    if (test){
      gen & fb=feuille._VECTptr->back();
      if (xcas_mode(contextptr)==3)
	return res+":Func "+fb.print(contextptr)+"\n:EndFunc\n";
      if (fb.is_symb_of_sommet(at_local)){
	gen fb0=fb._SYMBptr->feuille[0];
	//fb0=fb0[0];
	if (fb0.type==_VECT && fb0._VECTptr->empty())
	  return res+'{'+fb.print(contextptr)+'}';
      }
      return res+(fb.type==_VECT?printasbloc(fb,sommetstr,contextptr):fb.print(contextptr));
    }
    if (xcas_mode(contextptr)>0){
      if (xcas_mode(contextptr)==3)
	res+=":Func"+locals;
      else {
	res="proc("+proc_args.print(contextptr)+")"+locals;
	if (xcas_mode(contextptr)==2)
	  res +=indent(contextptr)+"begin ";
	if (inits.size()) 
	  res += indent(contextptr)+inits+";";
      }
    }
    else { 
      if (calc38)
	res += "BEGIN "+locals;
      else
	res += "{";
    }
    const_iterateur it=vect.begin(),itend=vect.end();
    debug_ptr(contextptr)->indent_spaces +=2;
    for (;;){
      if (xcas_mode(contextptr)==3)
	res += indent(contextptr)+it->print(contextptr);
      else
	res += indent(contextptr)+it->print(contextptr);
      ++it;
      if (it==itend){
	debug_ptr(contextptr)->indent_spaces -=2;
	if (xcas_mode(contextptr)!=3)
	  res += "; "+indent(contextptr);
	switch (xcas_mode(contextptr)){
	case 0:
	  res += calc38?"END;":"}";
	  break;
	case 1: case 1+_DECALAGE:
	  res+=indent(contextptr)+"end;";
	  break;
	case 2:
	  return res+=indent(contextptr)+"end_proc;";
	case 3:
	  return res+=indent(contextptr)+"EndFunc\n";
	}
	break;
      }
      else {
	if (xcas_mode(contextptr)!=3)
	  res +="; ";
      }
    }
    return res;
  }
  
  string rm_semi(const string & s){
    //return s;
    string res;
    for (int i=0;i<s.size();++i){
      if (s[i]!=';'){
	res += s[i];
	continue;
      }
      int j=res.size()-1;
      for (;j>=0;--j){
	if (res[j]!=' ')
	  break;
      }
      if (res[j]!=';' 
	  //&& res[j]!='}'
	  )
	res += ';';
    }
    return res;
  }

  static string printasprogram(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    string s=in_printasprogram(feuille,sommetstr,contextptr);
    return rm_semi(s);
  }

  static string texprintasprogram(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (latex_format(contextptr)==1){
      return printasprogram(feuille,sommetstr,contextptr);
    }
    string s("\\parbox{12cm}{\\tt ");
    s += translate_underscore(printasprogram(feuille,sommetstr,contextptr));
    s+=" }";
    return s;
  }

  // parser helper
  gen symb_test_equal(const gen & a,const gen & op,const gen & b){
    if (a.is_symb_of_sommet(at_and) && a._SYMBptr->feuille[1].is_symb_of_sommet(*op._FUNCptr)){
      vecteur v=*a._SYMBptr->feuille._VECTptr;
      v.push_back(symbolic(*op._FUNCptr,makesequence(v[1]._SYMBptr->feuille[1],b)));
      return symbolic(at_and,gen(v,_SEQ__VECT));
    }
    if (is_inequation(a) || a.is_symb_of_sommet(at_different) || (a.is_symb_of_sommet(at_same) && (b.type!=_INT_ || b.subtype!=_INT_BOOLEAN))){ 
      return symb_and(a,symbolic(*op._FUNCptr,gen(makevecteur(a._SYMBptr->feuille[1],b),_SEQ__VECT)));
    } 
    return symbolic(*op._FUNCptr,gen(makevecteur(a,b),_SEQ__VECT));  
  }

  // utility for default argument handling
  static void default_args(gen & a,gen & b,GIAC_CONTEXT){
#ifndef GIAC_DEFAULT_ARGS
    return;
#endif
    if (a.type==_VECT && b.type==_VECT && a._VECTptr->size()==b._VECTptr->size()){
      vecteur va=*a._VECTptr;
      vecteur vb=*b._VECTptr;
      for (unsigned i=0;i<a._VECTptr->size();++i)
	default_args(va[i],vb[i],contextptr);
      a=gen(va,a.subtype);
      b=gen(vb,b.subtype);
      return;
    }
    if (a.is_symb_of_sommet(at_sto)){
      b=a._SYMBptr->feuille[0];
      a=a._SYMBptr->feuille[1];
      return;
    }
    if (a.is_symb_of_sommet(at_equal)){
      b=a._SYMBptr->feuille[1];
      a=a._SYMBptr->feuille[0];
      return;
    }
    b=string2gen(gettext("Uninitialized parameter ")+a.print(contextptr),false);
    b.subtype=-1;
  }

  static void replace_keywords(const gen & a,const gen & b,gen & newa,gen & newb,GIAC_CONTEXT){
    // Check that variables in a are really variables, otherwise print
    // the var and make new variables
    vecteur newv(gen2vecteur(a));
    if (newv.size()==2 && newv.front().type==_VECT && newv.back().type==_VECT){
      gen tmpa,tmpb;
      replace_keywords(*newv.front()._VECTptr,b,tmpa,tmpb,contextptr);
      replace_keywords(*newv.back()._VECTptr,tmpb,newa,newb,contextptr);
      newa=gen(makevecteur(tmpa,newa),a.subtype);
      return;
    }
    vecteur v1,v2;
    iterateur it=newv.begin(),itend=newv.end();
    for (;it!=itend;++it){
      if (it->is_symb_of_sommet(at_sto) || it->is_symb_of_sommet(at_check_type) || it->is_symb_of_sommet(at_equal)) // FIXME check 1st arg too
	continue;
      if (it->is_symb_of_sommet(at_of)){
	*logptr(contextptr) << gettext("Invalid argument name ") << *it << gettext(". You should use ") << it->_SYMBptr->feuille._VECTptr->front() << gettext(" instead, even if the argument should be of type function") << '\n';
	*it=it->_SYMBptr->feuille._VECTptr->front();
      }
      if (it->is_symb_of_sommet(at_deuxpoints)){
	gen theid=it->_SYMBptr->feuille[0];
	gen egal=0;
	if (theid.is_symb_of_sommet(at_sto)){
	  egal=theid._SYMBptr->feuille[0];
	  theid=theid._SYMBptr->feuille[1];
	}
	if (theid.is_symb_of_sommet(at_equal)){
	  egal=theid._SYMBptr->feuille[1];
	  theid=theid._SYMBptr->feuille[0];
	}
	gen thetype=it->_SYMBptr->feuille[1],newid=*it;
	if (thetype.type==_INT_ && thetype.subtype==_INT_TYPE){
	  v1.push_back(theid);
	  // add = 
	  if (thetype.val==_ZINT){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_i"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  if (thetype.val==_DOUBLE_ || thetype.val==_REAL){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_d"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  if (thetype.val==_CPLX){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_c"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  if (thetype.val==_FRAC){
	    newid=*it=gen(identificateur(theid.print(contextptr)+"_f"));
	    if (egal!=0)
	      *it=symb_equal(*it,egal);
	  }
	  v2.push_back(newid);
	  continue;
	}
	if (thetype==at_real || thetype==at_float){
	  v1.push_back(theid);
	  *it=gen(identificateur(theid.print(contextptr)+"_d"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(*it);
	  continue;
	}
	if (thetype==at_complex){
	  v1.push_back(theid);
	  *it=gen(identificateur(theid.print(contextptr)+"_c"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(*it);
	  continue;
	}
	if (thetype==at_vector){
	  v1.push_back(theid);
	  newid=*it=gen(identificateur(theid.print(contextptr)+"_v"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(newid);
	  continue;
	}
	if (thetype==at_string){
	  v1.push_back(theid);
	  newid=*it=gen(identificateur(theid.print(contextptr)+"_s"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(newid);
	  continue;
	}
	if (thetype==at_integrate || thetype==at_int){ // int==integrate
	  v1.push_back(theid);
	  newid=*it=gen(identificateur(theid.print(contextptr)+"_i"));
	  if (egal!=0)
	    *it=symb_equal(*it,egal);
	  v2.push_back(newid);
	  continue;
	}
      }
      if (it->type!=_IDNT && it->type!=_CPLX){
	v1.push_back(*it);
	string s=gen2string(*it);
	int ss=int(s.size());
	if (ss==1)
	  s += "___";
	if (ss>2 && s[0]=='\'' && s[ss-1]=='\'')
	  s=s.substr(1,ss-2);
	for (unsigned i=0;i<s.size();++i){
	  if (!my_isalpha(s[i]))
	    s[i]='_';
	}
	lock_syms_mutex();  
	sym_string_tab::const_iterator i = syms().find(s);
	if (i == syms().end()) {
	  *it = *(new identificateur(s));
	  syms()[s] = *it;
	} else {
	  // std::cerr << "lexer" << s << '\n';
	  *it = i->second;
	}
	unlock_syms_mutex();  
	v2.push_back(*it);
      }
    }
    newa=gen(newv,_SEQ__VECT);
    if (v1.empty())
      newb=b;
    else {
      *logptr(contextptr) << gettext("Invalid or typed variable(s) name(s) were replaced by creating special identifiers, check ") << v1 << '\n';
      newb=quotesubst(b,v1,v2,contextptr);
    }
  }

  string mkvalid(const string & s){
    string res;
    for (size_t i=0;i<s.size();++i){
      char ch=s[i];
      if (ch!='.' && isalphan(ch)){
	res += ch;
	continue;
      }
      res += "char";
      res += char('0'+(ch/100));
      res += char('0'+((ch%100)/10));
      res += char('0'+(ch%10));
    }
    return res;
  }

  // a=arguments, b=values, c=program bloc, d=program name
  symbolic symb_program_sto(const gen & a_,const gen & b_,const gen & c_,const gen & d,bool embedd,GIAC_CONTEXT){
    gen a(a_),b(b_),c(c_);
    default_args(a,b,contextptr);
    bool warn=false;
#ifndef GIAC_HAS_STO_38
    if (logptr(contextptr) && calc_mode(contextptr)!=1)
      warn=warn_symb_program_sto; // true;
#endif
    if (warn){
      *logptr(contextptr) << gettext("// Parsing ") << d << '\n';
      lastprog_name(d.print(contextptr),contextptr);
      if (contains(a,vx_var) && c.type==_SYMB &&  c._SYMBptr->sommet!=at_local && c._SYMBptr->sommet!=at_bloc && (!lop(c,at_derive).empty() || !lop(c,at_integrate).empty())){
	*logptr(contextptr) << gettext("Warning, defining a function with a derivative/antiderivative should probably be done with ") <<  d << ":=unapply(" << c << "," << a << "). Evaluating for you.\n";
        c=eval(c,1,contextptr);
      }
      if (c.type==_SYMB && c._SYMBptr->sommet!=at_local && c._SYMBptr->sommet!=at_bloc && c._SYMBptr->sommet!=at_when && c._SYMBptr->sommet!=at_for && c._SYMBptr->sommet!=at_ifte){
	 vecteur lofc=lop(c,at_of);
	 vecteur lofc_no_d;
	 vecteur av=gen2vecteur(a);
	 for (unsigned i=0;i<lofc.size();++i){
	   if (lofc[i][1]!=d && !equalposcomp(av,lofc[i][1]) )
	     lofc_no_d.push_back(lofc[i]);
	 }
	 if (!lofc_no_d.empty()){
	   *logptr(contextptr) << gettext("Warning: algebraic function defined in term of others functions may lead to evaluation errors") << '\n';
	   CERR << c.print(contextptr) << '\n';
	   *logptr(contextptr) << gettext("Perhaps you meant ") << d.print(contextptr) << ":=unapply(" << c.print(contextptr) << ",";
	   if (a.type==_VECT && a.subtype==_SEQ__VECT && a._VECTptr->size()==1)
	     *logptr(contextptr) << a._VECTptr->front().print(contextptr) << ")" << '\n';
	   else
	     *logptr(contextptr) << a.print(contextptr) << ")" << '\n';
	 }
       }
    }
    vecteur newcsto(lop(c,at_sto)),newc1,newc2;
    for (size_t i=0;i<newcsto.size();++i){
      gen & val=newcsto[i]._SYMBptr->feuille._VECTptr->front();
      if (val.type==_VECT && (is_numericv(*val._VECTptr) || is_integer_vecteur(*val._VECTptr))) // in-place modification
	val=symbolic(at_copy,val);
      gen var=newcsto[i]._SYMBptr->feuille._VECTptr->back();
      if (var.type==_FUNC && (python_compat(contextptr) || !archive_function_index(*var._FUNCptr))){
	newc1.push_back(var);
	newc2.push_back(identificateur(mkvalid(var._FUNCptr->ptr()->print(contextptr))+"_rep"));
      }
    }
    newcsto=mergevecteur(lop(c,at_struct_dot),lop(c,at_for));
    for (size_t i=0;i<newcsto.size();++i){
      gen var=newcsto[i]._SYMBptr->feuille[0];
      if (var.type==_FUNC && var!=at_random && (python_compat(contextptr) || !archive_function_index(*var._FUNCptr))){
	newc1.push_back(var);
	newc2.push_back(identificateur(mkvalid(var._FUNCptr->ptr()->print(contextptr))+"_rep"));
      }
    }
    newcsto=gen2vecteur(a);
    for (size_t i=0;i<newcsto.size();++i){
      gen var=newcsto[i];
      if (var.type==_FUNC){
	newc1.push_back(var);
	newc2.push_back(identificateur(mkvalid(var._FUNCptr->ptr()->print(contextptr))+"_rep"));
      }
    }
    if (!newc1.empty()){
      c=subst(c,newc1,newc2,true,contextptr);
      a=subst(a,newc1,newc2,true,contextptr);
    }
    gen newa,newc;
    replace_keywords(a,((embedd&&c.type==_VECT)?makevecteur(c):c),newa,newc,contextptr);
    //cout << c._SYMBptr->sommet << '\n';
    bool cloc=newc.is_symb_of_sommet(at_local),glob=false;
    gen clocg;
    if (cloc){
      clocg=newc._SYMBptr->feuille;
      if (clocg.type==_VECT && !clocg._VECTptr->empty()){
	clocg=clocg._VECTptr->front();
	glob=clocg.type==_VECT && clocg._VECTptr->size()==2 && clocg._VECTptr->front().type==_VECT && clocg._VECTptr->front()._VECTptr->empty();
      }
    }
    if (python_compat(contextptr) && (!cloc || glob) ){
      vecteur res1,non_decl,res3,res4,Newa=gen2vecteur(newa);
      for (int i=0;i<int(Newa.size());++i){
	if (Newa[i].is_symb_of_sommet(at_equal))
	  Newa[i]=Newa[i]._SYMBptr->feuille[0];
      }
      check_local_assign(newc,Newa,res1,non_decl,res3,res4,false,contextptr);
      vecteur stov(lop(newc,at_sto));
      for (size_t i=0;i<stov.size();++i){
	stov[i]=stov[i]._SYMBptr->feuille[1];
      }
      vecteur stoprog(lop(newc,at_program));
      for (size_t i=0;i<stoprog.size();++i){
	stoprog[i]=stoprog[i]._SYMBptr->feuille[0];
      }
      vecteur stofor(lop(newc,at_for));
      for (size_t i=0;i<stofor.size();++i){
	stofor[i]=stofor[i]._SYMBptr->feuille[0];
      }
      stov=lidnt(mergevecteur(mergevecteur(stov,stoprog),stofor));
      int rs=int(non_decl.size());
      for (int i=0;i<rs;i++){
	// remove var that are not assigned (assumed global), constant idnt and recursive def
	gen noni=non_decl[i];
	if (noni.type!=_IDNT){
	  non_decl.erase(non_decl.begin()+i);
	  --i; --rs;
	  continue;
	}
	bool b=equalposcomp(stov,noni);
	if (strcmp(noni._IDNTptr->id_name,"i")==0){
	  if (!b){
	    non_decl.erase(non_decl.begin()+i);
	    --i; --rs;
	  }
	}
	else {
	  if (!b || is_constant_idnt(noni) || noni==d){ 
	    non_decl.erase(non_decl.begin()+i);
	    --i; --rs;
	  }
	}
      }
      if (!non_decl.empty()){
	*logptr(contextptr) << gettext("Auto-declared local variables : ") << gen(non_decl,_SEQ__VECT) << '\n';
	if (glob)
	  newc=symb_local(makesequence(non_decl,clocg._VECTptr->back()),newc._SYMBptr->feuille._VECTptr->back(),contextptr);
	else
	  newc=symb_local(non_decl,newc,contextptr);
      }
    }
    symbolic g=symbolic(at_program,gen(makevecteur(newa,b,newc),_SEQ__VECT));
    g=symbolic(at_sto,gen(makevecteur(g,d),_SEQ__VECT));
    if (warn)
      *logptr(contextptr) << check_local_assign(g,contextptr) ;
    if (warn && newc.is_symb_of_sommet(at_local)){
      // check that a local variable name does not shadow a parameter name
      gen & newcf=newc._SYMBptr->feuille;
      if (newcf.type==_VECT && newcf._VECTptr->size()==2){
	gen & vars = newcf._VECTptr->front(); // local vars, back is global vars
	gen inters=_intersect(gen(makevecteur(vars,newa),_SEQ__VECT),contextptr);
	if (inters.type==_VECT && !inters._VECTptr->empty()){
	  inters.subtype=_SEQ__VECT;
	  *logptr(contextptr) << gettext("Warning: Local variables shadow function arguments ") << inters << '\n';
	}
      }
    }
    if (printprog){
      int p=python_compat(contextptr);
      python_compat(printprog/256,contextptr);
      if (g.sommet==at_sto){
	lastprog_name(g.feuille[1].print(contextptr),contextptr);
	COUT << g.feuille[0].print(contextptr) <<'\n';	
      }
      else
	COUT << g <<'\n';
      python_compat(p,contextptr);
    }
    return g;
  }
  symbolic symb_program(const gen & a_,const gen & b_,const gen & c,GIAC_CONTEXT){
    gen a(a_),b(b_);
    default_args(a,b,contextptr);
    gen newa,newc;
    replace_keywords(a,c,newa,newc,contextptr);
    symbolic g=symbolic(at_program,gen(makevecteur(newa,b,newc),_SEQ__VECT));
#ifndef GIAC_HAS_STO_38
    if (logptr(contextptr))
      *logptr(contextptr) << check_local_assign(g,contextptr) ;
#endif
    return g;
  }
  symbolic symb_program(const gen & args){
    return symbolic(at_program,args);
  }
  static void lidnt_prog(const gen & g,vecteur & res);
  static void lidnt_prog(const vecteur & v,vecteur & res){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      lidnt_prog(*it,res);
  }
  static void lidnt_prog(const gen & g,vecteur & res){
    switch (g.type){
    case _VECT:
      lidnt_prog(*g._VECTptr,res);
      break;
    case _IDNT:
      if (!equalposcomp(res,g))
	res.push_back(g);
      break;
    case _SYMB:
      /* if (g._SYMBptr->sommet==at_at || g._SYMBptr->sommet==at_of )
	lidnt_prog(g._SYMBptr->feuille._VECTptr->back(),res);
	else */
	lidnt_prog(g._SYMBptr->feuille,res);
      break;
    }
  }

  static void local_vars(const gen & g,vecteur & v,GIAC_CONTEXT){
    if (g.type==_VECT){
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	local_vars(*it,v,contextptr);
      }
      return ;
    }
    if (g.type!=_SYMB)
      return;
    if (g._SYMBptr->sommet==at_local && g._SYMBptr->feuille.type==_VECT){
      vecteur & w = *g._SYMBptr->feuille._VECTptr;
      if (w[0].type==_VECT && w[0]._VECTptr->size()==2 && w[0]._VECTptr->front().type==_VECT)
	v=mergevecteur(v,*w[0]._VECTptr->front()._VECTptr);
      else
	v=mergevecteur(v,gen2vecteur(w[0]));
      local_vars(w[1],v,contextptr);
      return;
    }
    if (g._SYMBptr->sommet==at_program && g._SYMBptr->feuille.type==_VECT){
      vecteur & w = *g._SYMBptr->feuille._VECTptr;
      if (w[0].type==_VECT )
	v=mergevecteur(v,*w[0]._VECTptr);
    }
    local_vars(g._SYMBptr->feuille,v,contextptr);
  }

  gen quote_program(const gen & args,GIAC_CONTEXT){
    // return symb_program(args);
    // g:=unapply(p ->translation(w,p),w);g(1)
    // Necessite d'evaluer les arguments des programmes a l'interieur d'un programme.
    // Mais il ne faut pas evaluer les variables declarees comme locales!!
    bool in_prog=debug_ptr(contextptr)->sst_at_stack.size()!=0;
    // ?? Subst all variables except arguments
    if (!in_prog || args.type!=_VECT || args._VECTptr->size()!=3)
      return symb_program(args);
    vecteur & v = *args._VECTptr;
    vecteur vars(gen2vecteur(v[0]));
    int s=int(vars.size()); // s vars not subst-ed
    lidnt_prog(v[2],vars);
    vars=vecteur(vars.begin()+s,vars.end());
    // Remove local variables from the list
    vecteur tmpvar,resvar;
    local_vars(v[2],tmpvar,contextptr); 
    const_iterateur it=vars.begin(),itend=vars.end();
    for (;it!=itend;++it){
      if (!equalposcomp(tmpvar,*it))
	resvar.push_back(*it);
    }
    gen tmp=gen(resvar).eval(1,contextptr);
    vecteur varsub(*tmp._VECTptr);
    return symbolic(at_program,quotesubst(args,resvar,varsub,contextptr));
  }
  static bool is_return(const gen & g,gen & newres){
    if (g.type==_SYMB && g._SYMBptr->sommet==at_return){
      // gen tmp = g._SYMBptr->feuille; is_return(tmp,newres);
      is_return(g._SYMBptr->feuille,newres);
      return true;
    }
    if (g.type==_STRNG && g.subtype==-1){
      newres=g;
      return true;
    }
    if ( (g.type==_VECT && (g.subtype ==_SEQ__VECT || g.subtype==_PRG__VECT) && g._VECTptr->size()==1) )
      return is_return(g._VECTptr->front(),newres);
    newres=g;
    return false;
  }
  void adjust_sst_at(const gen & name,GIAC_CONTEXT){
    debug_ptr(contextptr)->sst_at.clear();
    const_iterateur it=debug_ptr(contextptr)->debug_breakpoint.begin(),itend=debug_ptr(contextptr)->debug_breakpoint.end();
    for (;it!=itend;++it){
      if (it->_VECTptr->front()==name)
	debug_ptr(contextptr)->sst_at.push_back(it->_VECTptr->back().val);
    }
  }

  void program_leave(const gen & save_debug_info,bool save_sst_mode,debug_struct * dbgptr){
    dbgptr->args_stack.pop_back();
    // *logptr(contextptr) << "Leaving " << args << '\n';
    if (!dbgptr->sst_at_stack.empty()){
      dbgptr->sst_at=dbgptr->sst_at_stack.back();
      dbgptr->sst_at_stack.pop_back();
    }
    if (!dbgptr->current_instruction_stack.empty()){
      dbgptr->current_instruction=dbgptr->current_instruction_stack.back();
      dbgptr->current_instruction_stack.pop_back();
    }
    dbgptr->sst_mode=save_sst_mode;
    if (dbgptr->current_instruction_stack.empty())
      dbgptr->debug_mode=false;
    (*dbgptr->debug_info_ptr)=save_debug_info;
    (*dbgptr->fast_debug_info_ptr)=save_debug_info;
  }

  gen _program(const gen & args,const gen & name,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return args.eval(prog_eval_level(contextptr),contextptr);
    // set breakpoints
    debug_struct * dbgptr=debug_ptr(contextptr);
    context * newcontextptr=(context *)contextptr;
    const_iterateur it,itend;
    gen res,newres,label,vars,values,prog,save_debug_info;
    // *logptr(contextptr) << & res << '\n';
#ifdef RTOS_THREADX
    if ((void *)&res<= (void *)&mainThreadStack[1024]) { 
      gensizeerr(gettext("Too many recursions"),res);
      return res;
    }
#else
#if !defined(WIN32) && defined HAVE_LIBPTHREAD
    if (contextptr){
      // CERR << &slevel << " " << thread_param_ptr(contextptr)->stackaddr << '\n';
      if ( ((size_t) &res) < ((size_t) thread_param_ptr(contextptr)->stackaddr)+8192){
	gensizeerr(gettext("Too many recursion levels"),res); 
	return res;
      }
    }
    else {
      if ( int(dbgptr->sst_at_stack.size()) >= MAX_RECURSION_LEVEL+1){
	gensizeerr(gettext("Too many recursions"),res);
	return res;
      }
    }
#else
    if ( int(dbgptr->sst_at_stack.size()) >= MAX_RECURSION_LEVEL+1){
      gensizeerr(gettext("Too many recursions"),res);
      return res;
    }
#endif
#endif
    dbgptr->sst_at_stack.push_back(dbgptr->sst_at);
    dbgptr->sst_at.clear();
    if (name.type==_IDNT)
      adjust_sst_at(name,contextptr);
    dbgptr->current_instruction_stack.push_back(dbgptr->current_instruction);
    dbgptr->current_instruction=0;
    bool save_sst_mode = dbgptr->sst_mode,findlabel,calc_save ;
    int protect=0;
    // *logptr(contextptr) << "Entering prog " << args << " " << dbgptr->sst_in_mode << '\n';
    if (dbgptr->sst_in_mode){
      dbgptr->sst_in_mode=false;
      dbgptr->sst_mode=true;
    }
    else
      dbgptr->sst_mode=false;
    // Bind local var
#ifdef TIMEOUT
    control_c();
#endif
    if (ctrl_c || interrupted || args._VECTptr->size()!=3){
      gensizeerr(res,contextptr);
      return res;
    }
    calc_save=calc_mode(contextptr)==38;
    if (calc_save) 
      calc_mode(-38,contextptr);
    vars=args._VECTptr->front();
    values=(*(args._VECTptr))[1];
    prog=args._VECTptr->back();
    save_debug_info=(*dbgptr->debug_info_ptr);
    if (vars.type!=_VECT)
      vars=gen(makevecteur(vars));
    if (values.type!=_VECT || values.subtype!=_SEQ__VECT || (vars._VECTptr->size()==1 && values._VECTptr->size()>1))
      values=gen(makevecteur(values));
    // *logptr(contextptr) << vars << " " << values << '\n';
    dbgptr->args_stack.push_back(gen(mergevecteur(vecteur(1,name),*values._VECTptr)));
    // removed sst test so that when a breakpoint is evaled
    // the correct info is displayed
    (*dbgptr->debug_info_ptr)=prog;
    (*dbgptr->fast_debug_info_ptr)=prog;
    if (!vars._VECTptr->empty())
      protect=giac_bind(*values._VECTptr,*vars._VECTptr,newcontextptr);
    if (protect==-RAND_MAX){
      program_leave(save_debug_info,save_sst_mode,dbgptr);
      if (calc_save) 
	calc_mode(38,contextptr);
      gensizeerr(res,contextptr);
      return res;
    }
#ifndef NO_STDEXCEPT
    try {
#endif
      if (prog.type!=_VECT || (prog.subtype && prog.subtype!=_PRG__VECT)){
	++debug_ptr(newcontextptr)->current_instruction;
	prog=equaltosto(prog,contextptr);
	if (debug_ptr(newcontextptr)->debug_mode){
	  debug_loop(res,newcontextptr);
	  if (!is_undef(res)){
	    if (!prog.in_eval(prog_eval_level(newcontextptr),res,newcontextptr))
	      res=prog;
	  }
	}
	else {
	  if (!prog.in_eval(prog_eval_level(newcontextptr),res,newcontextptr))
	    res=prog;
	}
	if (is_return(res,newres))
	  res=newres;
      }
      else {
	it=prog._VECTptr->begin();
	itend=prog._VECTptr->end();
	findlabel=false;
	for (;!ctrl_c && !interrupted && it!=itend;++it){
#ifdef TIMEOUT
	  control_c();
#endif
	  ++debug_ptr(newcontextptr)->current_instruction;
	  if (debug_ptr(newcontextptr)->debug_mode){
	    debug_loop(res,newcontextptr);
	    if (is_undef(res)) break;
	  }
	  if (!findlabel){
	    if (it->is_symb_of_sommet(at_return)){
	      if (!equaltosto(it->_SYMBptr->feuille,contextptr).in_eval(prog_eval_level(newcontextptr),newres,newcontextptr))
		newres=it->_SYMBptr->feuille;
	      is_return(newres,res);
	      break;
	    }
	    if (!equaltosto(*it,contextptr).in_eval(prog_eval_level(newcontextptr),res,newcontextptr))
	      res=*it;
	  }
	  else
	    res=*it;
	  if (res.type==_STRNG && res.subtype==-1)
	    break;
	  if (res.type==_SYMB){
	    if (findlabel && res.is_symb_of_sommet(at_label) && label==res._SYMBptr->feuille)
	      findlabel=false;
	    if (!findlabel && res.is_symb_of_sommet(at_goto)){
	      findlabel=true;
	      label=res._SYMBptr->feuille;
	    }
	  }
	  if (findlabel && it+1==itend)
	    it=prog._VECTptr->begin()-1;
	  if (!findlabel && is_return(res,newres)){
	    res=newres;
	    break;
	  }
	}
      }
#ifndef NO_STDEXCEPT
    } // end try
    catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      if (!vars._VECTptr->empty())
	leave(protect,*vars._VECTptr,newcontextptr);
      if (calc_save) 
	calc_mode(38,contextptr);
      throw(std::runtime_error(e.what()));
    }
#endif
    if (!vars._VECTptr->empty())
      leave(protect,*vars._VECTptr,newcontextptr);
    program_leave(save_debug_info,save_sst_mode,dbgptr);
    if (calc_save) 
      calc_mode(38,contextptr);
    return res;
  }
  static const char _program_s []="program";
  static define_unary_function_eval4_index (147,__program,&quote_program,_program_s,&printasprogram,&texprintasprogram);
  define_unary_function_ptr5( at_program ,alias_at_program,&__program,_QUOTE_ARGUMENTS,0);

  static string printasbloc(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( feuille.type!=_VECT || feuille._VECTptr->empty() )
      return "{"+feuille.print(contextptr)+";}";
    const_iterateur it=feuille._VECTptr->begin(),itend=feuille._VECTptr->end();
    string res("{");
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      res="\n";
      for (;it!=itend;++it){
	string add=it->print(contextptr);
	if (add.size() && add[0]=='\n' && res.size() && res[res.size()-1]=='\n')
	  res += add.substr(1,add.size()-1)+'\n';
	else
	  res += string(ind,' ')+add+'\n';
      }
      return res;
    }
    if (xcas_mode(contextptr)>0){
      if (xcas_mode(contextptr)==3)
	res="";
      else
	res=indent(contextptr)+"begin";
    }
    debug_ptr(contextptr)->indent_spaces +=2;
    for (;;){
      if (xcas_mode(contextptr)==3)
	res += indent(contextptr)+it->print(contextptr);
      else
	res += indent(contextptr)+it->print(contextptr);
      ++it;
      if (it==itend){
	debug_ptr(contextptr)->indent_spaces -=2;
	if (xcas_mode(contextptr)==3)
	  break;
	res += "; "+indent(contextptr);
	if (xcas_mode(contextptr)>0)
	  res += indent(contextptr)+"end";
	else
	  res += "}";
	break;
      }
      else {
	if (xcas_mode(contextptr)!=3)
	  res +="; ";
      }
    }
    return rm_semi(res);
  }
  gen symb_bloc(const gen & args){
    if (args.type!=_VECT)
      return args;
    if (args.type==_VECT && args._VECTptr->size()==1)
      return args._VECTptr->front();
    gen a=args; a.subtype=_SEQ__VECT;
    return symbolic(at_bloc,a);
  }
  gen _bloc(const gen & prog,GIAC_CONTEXT){
    if ( prog.type==_STRNG &&  prog.subtype==-1) return  prog;
    gen res,label;
    bool findlabel=false;
    debug_struct * dbgptr=debug_ptr(contextptr);
    if (prog.type!=_VECT){
      ++dbgptr->current_instruction;
      if (dbgptr->debug_mode){
	debug_loop(res,contextptr);
	if (is_undef(res)) return res;
      }
      return prog.eval(eval_level(contextptr),contextptr);
    }
    else {
      const_iterateur it=prog._VECTptr->begin(),itend=prog._VECTptr->end();
      for (;!ctrl_c && !interrupted && it!=itend;++it){
#ifdef TIMEOUT
	control_c();
#endif
	++dbgptr->current_instruction;
	if (dbgptr->debug_mode){
	  debug_loop(res,contextptr);
	  if (is_undef(res)) return res;
	}
	if (!findlabel){
	  if (it->type==_SYMB && it->_SYMBptr->sommet==at_return){
	    // res=it->_SYMBptr->feuille.eval(prog_eval_level(contextptr),contextptr);
	    if (!it->_SYMBptr->feuille.in_eval(prog_eval_level(contextptr),res,contextptr))
	      res=it->_SYMBptr->feuille;
	    increment_instruction(it+1,itend,dbgptr);
	    return symbolic(at_return,res);
	  }
	  else {
	    // res=it->eval(eval_level(contextptr),contextptr);
	    if (!equaltosto(*it,contextptr).in_eval(eval_level(contextptr),res,contextptr))
	      res=*it;
	  }
	}
	else 
	  res=*it;
	if (res.type==_STRNG && res.subtype==-1)
	  return res;
	if (res.type==_SYMB){
	  unary_function_ptr & u=res._SYMBptr->sommet;
	  if (!findlabel && (u==at_return || u==at_break)) {
	    increment_instruction(it+1,itend,dbgptr);
	    return res; // it->eval(eval_level(contextptr),contextptr);
	  }
	  if (!findlabel && u==at_goto){
	    findlabel=true;
	    label=res._SYMBptr->feuille;
	  }
	  if ( u==at_label && label==res._SYMBptr->feuille )
	    findlabel=false;
	}
	// restart the bloc if needed
	if (findlabel && it+1==itend)
	  it=prog._VECTptr->begin()-1;
      }
    }
    return res;
  }
  static const char _bloc_s []="bloc";
  static define_unary_function_eval2_index (145,__bloc,&_bloc,_bloc_s,&printasbloc);
  define_unary_function_ptr5( at_bloc ,alias_at_bloc,&__bloc,_QUOTE_ARGUMENTS,0);

  // test
  string printasifte(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=3) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    bool calc38=abs_calc_mode(contextptr)==38;
    const_iterateur it=feuille._VECTptr->begin();//,itend=feuille._VECTptr->end();
    string res(calc38?"IF ":"if ");
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      res = '\n'+string(ind,' ')+string("if ")+it->print(contextptr)+" :\n";
      ind += 4;
      ++it;
      res += string(ind,' ')+it->print(contextptr);
      ++it;
      if (it->type!=_INT_)
	res += '\n'+string(ind-4,' ')+string("else :\n")+string(ind,' ')+it->print(contextptr);
      ind -= 4;
      return res;
    }
    if (xcas_mode(contextptr)==3)
      res="If ";
    if (calc38 || xcas_mode(contextptr)>0)
      res += sametoequal(*it).print(contextptr);
    else
      res += "("+it->print(contextptr);
    ++it;
    if (calc38 || xcas_mode(contextptr)>0){
      if (xcas_mode(contextptr)==3){
	if (is_undef(*(it+1)) && (it->type!=_SYMB || it->_SYMBptr->sommet!=at_bloc) )
	  return res + indent(contextptr)+"  "+it->print(contextptr);
	res += " Then "; // +indent(contextptr);
      }
      else
	res += calc38?" THEN ":" then ";
    }
    else
      res += ") ";
    debug_ptr(contextptr)->indent_spaces +=2;
    if ((calc38 || xcas_mode(contextptr)>0) && (it->type==_SYMB) && (it->_SYMBptr->sommet==at_bloc))
      res += printasinnerbloc(it->_SYMBptr->feuille,contextptr);
    else {
      res += it->print(contextptr);
      char reslast=res[res.size()-1];
      if (reslast!=';' && reslast!='}') 
	res += ";";
    }
    debug_ptr(contextptr)->indent_spaces -=2;
    ++it;
    while ( (calc38 || xcas_mode(contextptr)>0) && (it->type==_SYMB) && (it->_SYMBptr->sommet==at_ifte) ){
      if (xcas_mode(contextptr)==3)
	res += indent(contextptr)+"Elseif ";
      else
	res += indent(contextptr)+ (calc38? "ELIF ":"elif ");
      it=it->_SYMBptr->feuille._VECTptr->begin();
      res += it->print(contextptr);
      if (xcas_mode(contextptr)==3)
	res += " Then "; // +indent(contextptr);
      else
	res += calc38? " THEN ":" then ";
      ++it;
      debug_ptr(contextptr)->indent_spaces +=2;
      if ((it->type==_SYMB) && (it->_SYMBptr->sommet==at_bloc))
	res += printasinnerbloc(it->_SYMBptr->feuille,contextptr);
      else {
	res += it->print(contextptr);
	if (res[res.size()-1]!=';') 
	  res += ";";
      }
      debug_ptr(contextptr)->indent_spaces -=2;
      ++it;
    }
    if (!is_zero(*it)){
      if (!calc38 && xcas_mode(contextptr)<=0 && (res[res.size()-1]=='}'))
	res += indent(contextptr)+" else ";
      else {
	if (calc38)
	  res += " ELSE ";
	else {
	  if (xcas_mode(contextptr)<=0 && res[res.size()-1]!=';')
	    res +=";"; 
	  if (xcas_mode(contextptr)==3)
	    res += indent(contextptr)+"Else ";
	  else
	    res+= " else ";
	}
      }
      debug_ptr(contextptr)->indent_spaces +=2;
      if ((calc38 || xcas_mode(contextptr)>0) && (it->type==_SYMB) && (it->_SYMBptr->sommet==at_bloc))
	res += printasinnerbloc(it->_SYMBptr->feuille,contextptr);
      else {
	if (it->type==_SYMB && (it->_SYMBptr->sommet==at_ifte || it->_SYMBptr->sommet==at_for)){
	  res +='{';
	  res += it->print(contextptr);
	  res += '}';
	}
	else {
	  res += it->print(contextptr);
	  if (res[res.size()-1]!=';' 
	      //&& !xcas_mode(contextptr)
	      )
	    res += ";";
	}
      }
      debug_ptr(contextptr)->indent_spaces -=2;
    }
    // FIXME? NO ; AT END OF IF
    if ((xcas_mode(contextptr)<=0) && (res[res.size()-1]!='}'))
      res +=" ";
    if (calc38)
      res += " END ";
    else {
      if ( (xcas_mode(contextptr) ==1) || (xcas_mode(contextptr) == 1+_DECALAGE) )
	res += indent(contextptr)+ "fi ";
      if (xcas_mode(contextptr)==2)
	res += indent(contextptr)+ "end_if ";
      if (xcas_mode(contextptr)==3)
	res += indent(contextptr)+"EndIf ";
    }
    return res;
  }
  symbolic symb_ifte(const gen & test,const gen & oui, const gen & non){
    return symbolic(at_ifte,gen(makevecteur(test,oui,non),_SEQ__VECT));
  }

  gen symb_return(const gen & arg){
    return gen(symbolic(at_return,arg));
  }
  gen symb_when(const gen & arg){
    return gen(symbolic(at_when,arg));
  }

  gen ifte(const gen & args,bool isifte,const context * contextptr){
    gen test,res;
    if (args.type!=_VECT || args._VECTptr->size()!=3){
      gensizeerr(gettext("Ifte must have 3 args"),res);
      return res;
    }
    int evallevel=eval_level(contextptr);
#if 1
    res=args._VECTptr->front();
    if (!res.in_eval(evallevel,test,contextptr))
      test=res;
    if (test.type!=_INT_){
      test=equaltosame(test).eval(evallevel,contextptr);
      if (!is_integer(test)){
	test=test.type==_MAP?!test._MAPptr->empty():test.evalf_double(evallevel,contextptr);
	if (test.type==_VECT && python_compat(contextptr)){
	  // OR test on a list
	  const_iterateur it=test._VECTptr->begin(),itend=test._VECTptr->end();
	  for (;it!=itend;++it){
	    if (!is_zero(*it,contextptr)){
	      test=*it;
	      break;
	    }
	  }
	  if (it==itend)
	    test=0;
	}
	if ( test.type>_CPLX ){
	  if (isifte){
	    gensizeerr(gettext("Ifte: Unable to check test"),res); 
	    return res;
	  }
	  else
	    return symb_when(eval(args,1,contextptr));
	}
      }
    }
#else
    test=args._VECTptr->front();
    test=equaltosame(test.eval(eval_level(contextptr),contextptr)).eval(eval_level(contextptr),contextptr);
    if (!is_integer(test)){
      test=test.evalf_double(eval_level(contextptr),contextptr);
      if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) ){
	if (isifte){
	  gensizeerr(gettext("Ifte: Unable to check test"),res); 
	  return res;
	}
	else
	  return symb_when(eval(args,1,contextptr));
      }
    }
#endif
    bool rt;
    vecteur *argsptr=args._VECTptr;
    // *logptr(contextptr) << "Ifte " << debug_ptr(contextptr)->current_instruction << '\n' ;
    if (is_zero(test)){ // test false, do the else part
      if (isifte){
	debug_struct * dbgptr=debug_ptr(contextptr);
	increment_instruction((*argsptr)[1],dbgptr);
	// *logptr(contextptr) << "Else " << debug_ptr(contextptr)->current_instruction << '\n' ;
	++dbgptr->current_instruction;
	if (dbgptr->debug_mode){
	  debug_loop(test,contextptr);
	  if (is_undef(test)) return test;
	}
      }
      if (argsptr->back().type==_INT_)
	return argsptr->back();
      gen clause_fausse=equaltosto(argsptr->back(),contextptr);
      rt=clause_fausse.is_symb_of_sommet(at_return);
      if (rt)
	clause_fausse=clause_fausse._SYMBptr->feuille;
      // res=clause_fausse.eval(evallevel,contextptr);
      if (!clause_fausse.in_eval(evallevel,res,contextptr))
	res=clause_fausse;
      if (rt && (res.type!=_SYMB || res._SYMBptr->sommet!=at_return))
	res=symb_return(res);
      // *logptr(contextptr) << "Else " << debug_ptr(contextptr)->current_instruction << '\n' ;
    }
    else { // test true, do the then part
      if (isifte){
	debug_struct * dbgptr=debug_ptr(contextptr);
	++dbgptr->current_instruction;
	if (dbgptr->debug_mode){
	  debug_loop(test,contextptr);
	  if (is_undef(test)) return test;
	}
      }
      gen clause_vraie=equaltosto((*argsptr)[1],contextptr);
      rt=clause_vraie.is_symb_of_sommet(at_return);
      if (rt)
	clause_vraie=clause_vraie._SYMBptr->feuille;
      // res=clause_vraie.eval(evallevel,contextptr);
      if (!clause_vraie.in_eval(evallevel,res,contextptr))
	res=clause_vraie;
      if (rt && (res.type!=_SYMB || res._SYMBptr->sommet!=at_return) )
	res=symb_return(res);
      // *logptr(contextptr) << "Then " << debug_ptr(contextptr)->current_instruction << '\n' ;
      if (isifte)
	increment_instruction(argsptr->back(),contextptr);
      // *logptr(contextptr) << "Then " << debug_ptr(contextptr)->current_instruction << '\n' ;
    }
    return res;
  }
  gen _ifte(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return ifte(args,true,contextptr);
  }
  static const char _ifte_s []="ifte";
  static define_unary_function_eval2_index (141,__ifte,&_ifte,_ifte_s,&printasifte);
  define_unary_function_ptr5( at_ifte ,alias_at_ifte,&__ifte,_QUOTE_ARGUMENTS,0);

  gen _evalb(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT) return apply(args,_evalb,contextptr);
    gen test=equaltosame(args);
    test=normal(test,contextptr);
    test=test.eval(eval_level(contextptr),contextptr);
    test=test.evalf_double(1,contextptr);
    if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) ){
      test=args.eval(eval_level(contextptr),contextptr);
      test=equaltosame(test);
      test=normal(test,contextptr);
      test=test.eval(eval_level(contextptr),contextptr);
      test=test.evalf_double(1,contextptr);
      if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) ){
	return symbolic(at_evalb,args);
      }
    }
    gen g=is_zero(test)?zero:plus_one;
    g.subtype=_INT_BOOLEAN;
    return g;
  }
  static const char _evalb_s []="evalb";
  static define_unary_function_eval_quoted (__evalb,&_evalb,_evalb_s);
  define_unary_function_ptr5( at_evalb ,alias_at_evalb,&__evalb,_QUOTE_ARGUMENTS,true);

  static const char _maple_if_s []="if";
  static define_unary_function_eval2_quoted (__maple_if,&_ifte,_maple_if_s,&printasifte);
  define_unary_function_ptr5( at_maple_if ,alias_at_maple_if,&__maple_if,_QUOTE_ARGUMENTS,0);

  static string printaswhen(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    bool b=calc_mode(contextptr)==38;
    if (b || xcas_mode(contextptr)|| feuille.type!=_VECT || feuille._VECTptr->size()!=3)
      return (b?"IFTE":sommetstr)+("("+feuille.print(contextptr)+")");
    vecteur & v=*feuille._VECTptr;
    if (calc_mode(contextptr)==1 || python_compat(contextptr)){
#if 0
      string s="If["+v[0].print(contextptr)+","+v[1].print(contextptr);
      if (!is_undef(v[2]))
	s +=","+v[2].print(contextptr);
      return s+"]";
#else
      string s="when("+v[0].print(contextptr)+","+v[1].print(contextptr)+","+v[2].print(contextptr)+")";
      return s;
#endif
    }
    return "(("+v[0].print(contextptr)+")? "+v[1].print(contextptr)+" : "+v[2].print(contextptr)+")";
  }
  gen symb_when(const gen & t,const gen & a,const gen & b){
    return symbolic(at_when,gen(makevecteur(t,a,b),_SEQ__VECT));
  }
  gen _when(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(gettext("3 or 4 arguments expected"));
    vecteur & v=*args._VECTptr;
    if (v.size()==3){
      gen res=ifte(args,false,contextptr);
      return res;
    }
    if (v.size()!=4)
      return gentypeerr(contextptr);
    gen res=ifte(vecteur(v.begin(),v.begin()+3),false,contextptr);
    if (res.type==_SYMB && res._SYMBptr->sommet==at_when)
      return v[3];
    return res;
  }
  static const char _when_s []="when";
  static define_unary_function_eval2_quoted (__when,&_when,_when_s,&printaswhen);
  define_unary_function_ptr5( at_when ,alias_at_when,&__when,_QUOTE_ARGUMENTS,true);

  // loop
  string printasfor(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_VECT) 
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    if (feuille._VECTptr->size()==2)
      return feuille._VECTptr->front().print(contextptr)+ " in "+feuille._VECTptr->back().print(contextptr);
    if (feuille._VECTptr->size()!=4)
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    int maplemode=xcas_mode(contextptr) & 0x07;
    if (abs_calc_mode(contextptr)==38)
      maplemode=4;
    const_iterateur it=feuille._VECTptr->begin();//,itend=feuille._VECTptr->end();
    string res;
    gen inc(from_increment(*(it+2)));
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      if (inc.is_symb_of_sommet(at_sto) && inc._SYMBptr->feuille[1].type==_IDNT){
	gen index=inc._SYMBptr->feuille[1];
	if (inc._SYMBptr->feuille[0]==index+1 && it->is_symb_of_sommet(at_sto) && it->_SYMBptr->feuille[1]==index){
	  gen start=it->_SYMBptr->feuille[0];
	  if ((it+1)->type==_SYMB && ((it+1)->_SYMBptr->sommet==at_inferieur_egal || (it+1)->_SYMBptr->sommet==at_inferieur_strict) && (it+1)->_SYMBptr->feuille[0]==index){
	    int large=(it+1)->_SYMBptr->sommet==at_inferieur_egal?1:0;
	    gen stop=(it+1)->_SYMBptr->feuille[1];
	    res +=  '\n'+string(ind,' ')+string("for ")+index.print(contextptr)+" in range(";
	    if (start!=0)
	      res +=start.print(contextptr)+",";
	    res +=(stop+large).print(contextptr)+"):"+'\n';
	    ind += 4;
	    res += string(ind,' ')+(it+3)->print(contextptr);
	    ind -=4;
	    return res;
	  }
	}
      }
      if (it->type!=_INT_) res += '\n'+string(ind,' ')+it->print(contextptr)+'\n';
      res += '\n'+string(ind,' ')+string("while ") + (it+1)->print(contextptr)+" :";
      if (!(it+3)->is_symb_of_sommet(at_bloc))
	res += '\n';
      ind += 4;
      res += string(ind,' ')+(it+3)->print(contextptr);
      if ((it+2)->type!=_INT_) res += '\n'+string(ind,' ')+(it+2)->print(contextptr);      
      ind -=4;
      return res;
    }
    if (is_integer(*it) && is_integer(*(it+2))){
      ++it;
      if (is_one(*it) && (it+2)->is_symb_of_sommet(at_bloc)){
	const gen & loopf=(it+2)->_SYMBptr->feuille;
	if (loopf.type==_VECT && loopf._VECTptr->back().is_symb_of_sommet(at_ifte)){
	  const vecteur & condv=*loopf._VECTptr->back()._SYMBptr->feuille._VECTptr;
	  if (condv.size()==3 && is_zero(condv[2]) && condv[1].is_symb_of_sommet(at_break)){ 
	    // repeat until condv[0] loop, loopf except the end is the loop
	    vecteur corps=vecteur(loopf._VECTptr->begin(),loopf._VECTptr->end()-1);
	    res = maplemode==4?"REPEAT ":"repeat ";
	    res += printasinnerbloc(corps,contextptr);
	    res += indent(contextptr);
	    res += maplemode==4?"UNTIL ":"until ";
	    res += condv[0].print();
	    return res;
	  }
	}
      }
      bool dodone=false;
      if (it->is_symb_of_sommet(at_for) && it->_SYMBptr->feuille.type==_VECT && it->_SYMBptr->feuille._VECTptr->size()==2){
	res = "for "+it->_SYMBptr->feuille._VECTptr->front().print(contextptr)+" in "+ it->_SYMBptr->feuille._VECTptr->back().print(contextptr)+ " do ";
	dodone=true;
      }
      else {
	if (maplemode>0){
	  if (maplemode==3 && is_one(*it) ){
	    it += 2;
	    res="Loop";
	    if ((it->type==_SYMB) && (it->_SYMBptr->sommet==at_bloc))
	      res += printasinnerbloc(it->_SYMBptr->feuille,contextptr);
	    else
	      res += it->print(contextptr) +";";
	    return res+indent(contextptr)+"'EndLoop";
	  }
	  if (maplemode==3)
	    res = "While "+ sametoequal(*it).print(contextptr) +indent(contextptr);
	  else
	    res = (maplemode==4?"WHILE ":"while ") + sametoequal(*it).print(contextptr) + (maplemode==4?" DO":" do ");
	}
	else
	  res = "while("+it->print(contextptr)+")";
      }
      ++it;
      ++it;
      debug_ptr(contextptr)->indent_spaces += 2;
      if ((maplemode>0 || res.substr(res.size()-3,3)=="do ") && (it->type==_SYMB) && (it->_SYMBptr->sommet==at_bloc))
	res += printasinnerbloc(it->_SYMBptr->feuille,contextptr)+";";
      else
	res += it->print(contextptr) +";";
      debug_ptr(contextptr)->indent_spaces -= 2;
      if (maplemode==1 || dodone)
	return res+indent(contextptr)+" od;";
      if (maplemode==2)
	return res+indent(contextptr)+" end_while;";
      if (maplemode==3)
	return res+indent(contextptr)+" EndWhile";
      if (maplemode==4)
	return res+indent(contextptr)+" END;";
    }
    else {  
      if (maplemode>0){// pb for generic loops for Maple translation
	gen inc=from_increment(*(it+2));
	if ( (it->type!=_SYMB) || (it->_SYMBptr->sommet!=at_sto) || (inc.type!=_SYMB) || inc._SYMBptr->sommet!=at_sto || (it->_SYMBptr->feuille._VECTptr->back()!=inc._SYMBptr->feuille._VECTptr->back()) )
	  return gettext("Maple/Mupad/TI For: unable to convert");
	gen var_name=it->_SYMBptr->feuille._VECTptr->back();
	gen step=normal(inc._SYMBptr->feuille._VECTptr->front()-var_name,contextptr);
	gen condition=*(it+1),limite=plus_inf;
	if (is_positive(-step,contextptr)) 
	  limite=minus_inf;
	bool simple_loop=false,strict=true,ascending=true;
	if (condition.type==_SYMB){
	  unary_function_ptr op=condition._SYMBptr->sommet;
	  if (condition._SYMBptr->feuille.type==_VECT){
	    if (op==at_inferieur_strict)
	      simple_loop=true;
	    if (op==at_inferieur_egal){
	      strict=false;
	      simple_loop=true;
	    }
	    if (op==at_superieur_strict){
	      simple_loop=(maplemode>=2);
	      ascending=false;
	    }
	    if (op==at_superieur_egal){
	      simple_loop=(maplemode>=2);
	      ascending=false;
	      strict=false;
	    }
	  }
	  if (simple_loop){
	    simple_loop=(condition._SYMBptr->feuille._VECTptr->front()==var_name);
	    limite=condition._SYMBptr->feuille._VECTptr->back();
	  }
	}
	if (maplemode==3)
	  res="For ";
	else
	  res = (maplemode==4?"FOR ":"for ");
	res += var_name.print(contextptr);
	if (maplemode==3)
	  res += ",";
	else
	  res += (maplemode==4?" FROM ":" from ");
	res += it->_SYMBptr->feuille._VECTptr->front().print(contextptr);
	if (maplemode==3){
	  res += ","+limite.print(contextptr);
	  if (!is_one(step))
	    res += ","+step.print(contextptr);
	  if (!simple_loop)
	    res += indent(contextptr)+"If not("+(it+1)->print(contextptr)+")"+indent(contextptr)+"Exit";
	  res += indent(contextptr);
	}
	else {
	  gen absstep=step;
	  if (simple_loop){
	    absstep = abs(step,contextptr); 
	    if (ascending)
	      res += maplemode==4?" TO ":" to ";
	    else
	      res += maplemode==4?" DOWNTO ":" downto ";
	    res += limite.print(contextptr);
#ifndef BCD
	    if (!strict && !is_integer(step)){
	      if (ascending)
		res +="+";
	      else
		res += "-";
	      res += absstep.print(contextptr);
	      res += "/2";
	    }
#endif
	  }
	  if (!is_one(absstep)){
	    if (maplemode==2)
	      res += " step ";
	    else
	      res += maplemode==4?" STEP ":" by ";
	    res += step.print(contextptr);
	  }
	  if (!simple_loop){
	    res += (maplemode==4)?" WHILE ":" while ";
	    res += (it+1)->print(contextptr);
	  }
	  res += maplemode==4?" DO ":" do ";
	}
	it += 3;
	if ((it->type==_SYMB) && (it->_SYMBptr->sommet==at_bloc))
	  res += printasinnerbloc(it->_SYMBptr->feuille,contextptr)+";";
	else
	  res += it->print(contextptr)+";" ;
	if (maplemode==1)
	  return res + indent(contextptr)+" od;";
	if (maplemode==2)
	  return res + indent(contextptr)+" end_for;";
	if (maplemode==3)
	  return res + indent(contextptr)+"EndFor";
	if (maplemode==4)
	  return res + indent(contextptr)+"END;";
      }
      res="for (";
      res += it->print(contextptr) + ';';
      ++it;
      res += it->print(contextptr) + ';';
      ++it;
      res += it->print(contextptr) + ") ";
      ++it;
      debug_ptr(contextptr)->indent_spaces += 2;
      if (it->is_symb_of_sommet(at_bloc))
	res += it->print(contextptr) ;
      else {
	res += '{';
	res += it->print(contextptr) ;
	res += '}';
      }
      debug_ptr(contextptr)->indent_spaces -= 2;
    }
    if (res[res.size()-1]!='}')
      res += "; ";
    return res;
  }
  symbolic symb_for(const gen & e){
    return symbolic(at_for,e);
  }
  symbolic symb_for(const gen & a,const gen & b,const gen & c,const gen & d){
    return symbolic(at_for,gen(makevecteur(a,b,c,d),_SEQ__VECT));
  }
  
  static gen to_increment(const gen & g){
    if (!g.is_symb_of_sommet(at_sto))
      return g;
    gen & f =g._SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=2)
      return g;
    gen & a = f._VECTptr->front();
    gen & b = f._VECTptr->back();
    if (b.type!=_IDNT || a.type!=_SYMB)
      return g;
    gen & af=a._SYMBptr->feuille;
    if (af.type!=_VECT || af._VECTptr->empty())
      return g;
    vecteur & av= *af._VECTptr;
    int s=int(av.size());
    int type=0;
    if (a.is_symb_of_sommet(at_plus))
      type=1;
    // there was a wrong test with at_minus for -= (type=-1)
    if (type && av.front()==b){
      if (s==2){
	if (is_one(av.back()))
	  return symbolic(type==1?at_increment:at_decrement,b);
	if (is_minus_one(av.back()))
	  return symbolic(type==1?at_decrement:at_increment,b);
	return symbolic(type==1?at_increment:at_decrement,gen(makevecteur(b,av.back()),_SEQ__VECT));
      }
      if (type)
	return symbolic(at_increment,gen(makevecteur(b,symbolic(at_plus,vecteur(av.begin()+1,av.end()))),_SEQ__VECT));
    }
    return g;
  }
  static bool ck_is_one(gen & g,GIAC_CONTEXT){
    if (is_one(g))
      return true;
    if (g.type==_VECT && python_compat(contextptr)){
      // OR test on a list
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	if (!is_zero(*it,contextptr))
	  return true;
      }
      return false;
    }
    if (g.type>_POLY){
      g=gensizeerr(gettext("Unable to eval test in loop : ")+g.print());
      return false; // this will stop the loop in caller
    }
    return false;
  }
  static bool set_for_in(int counter,int for_in,vecteur * v,string * s,const gen & name,GIAC_CONTEXT){
    int taille=int(v?v->size():s->size());
    if (counter>0 && counter<=taille){
      if (v)
	(*v)[counter-1]=eval(name,1,contextptr);
      else {
	if (s){	 
	  gen g=eval(name,1,contextptr);
	  if (g.type==_STRNG && !g._STRNGptr->empty())
	    (*s)[counter-1]=(*g._STRNGptr)[0];
	}
      }
    }
    if (counter<0 || counter>=taille)
      return false;
    gen res;
    if (v){
      res=(*v)[counter];
      res=sto(res,name,contextptr);
    }
    else {
      char ch=(*s)[counter];
      res=sto(string2gen(string(1,ch),false),name,contextptr);
    }
    return !is_undef(res);
  }
  inline bool for_test(const gen & test,gen & testf,int eval_lev,context * contextptr){
    // ck_is_one( (testf=test.eval(eval_lev,newcontextptr).evalf(1,newcontextptr)) )
    if (!test.in_eval(eval_lev,testf,contextptr)) 
      testf=test;
    //testf=test.eval(eval_lev,contextptr);
    if (testf.type==_INT_) 
      return testf.val;
    if (testf.type==_FRAC) 
      testf=testf._FRACptr->num;
    if (test.type<=_POLY)
      return !is_exactly_zero(test);
    if (testf.type==_MAP)
      return !testf._MAPptr->empty();
    testf=testf.evalf(1,contextptr);
    return ck_is_one(testf,contextptr);
  }

  // return false if forprog modifies index or stopg
  bool chk_forprog(const gen & forprog,const gen & index,const gen & stopg);
  bool chk_forprog(const vecteur & forprog,const gen & index,const gen & stopg){
    const_iterateur it=forprog.begin(),itend=forprog.end();
    for (;it!=itend;++it){
      if (!chk_forprog(*it,index,stopg))
	return false;
    }
    return true;
  }
  bool chk_forprog(const gen & forprog,const gen & index,const gen & stopg){
    if (forprog.type==_VECT)
      return chk_forprog(*forprog._VECTptr,index,stopg);
    if (forprog.type!=_SYMB || forprog.subtype==_FORCHK__SYMB)
      return true;
    unary_function_ptr & u=forprog._SYMBptr->sommet;
    if (u==at_sto || u==at_array_sto){
      const gen * to=&(*forprog._SYMBptr->feuille._VECTptr)[1];
      if (*to==index || *to==stopg)
	return false;
    }
    if (u==at_increment || u==at_decrement){
      const gen * to=&forprog._SYMBptr->feuille;
      if (to->type==_VECT) to=&to->_VECTptr->front();
      if (*to==index || *to==stopg) 
	return false;
    }
    if (!chk_forprog(forprog._SYMBptr->feuille,index,stopg))
      return false;
    ((gen *) &forprog)->subtype=_FORCHK__SYMB;
    return true;
  }
  gen _for(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    // for elem in list: for(elem,list), inert form
    if (args.type!=_VECT || args._VECTptr->size()==2)
      return symb_for(args);
    const vecteur & argsv = *args._VECTptr;
    if (argsv.size()!=4)
      return gensizeerr(gettext("For must have 4 args"));
    // Initialization
    gen initialisation=argsv.front(),forelse;
    // add assigned variables to be local
    bool bound=false;
    vecteur loop_var;
    int protect=0;
    context * newcontextptr=(context * ) contextptr;
    if ( (initialisation.type==_SYMB) && (initialisation._SYMBptr->sommet==at_sto)){
      gen variable=initialisation._SYMBptr->feuille._VECTptr->back();
      if (variable.type==_IDNT){
	if (contextptr==context0 && (xcas_mode(contextptr)!=1 && (!variable._IDNTptr->localvalue || variable._IDNTptr->localvalue->empty() || (*variable._IDNTptr->localvalue)[variable._IDNTptr->localvalue->size()-2].val<protection_level-1) ) ){
	  bound=true;
	  loop_var=makevecteur(variable);
	  protect=giac_bind(makevecteur(zero),loop_var,newcontextptr);
	}
      }
      else {
#ifndef NO_STDEXCEPT
	throw(std::runtime_error(gettext("Invalid loop index (hint: i=sqrt(-1)!)")));
#endif
	return undeferr(gettext("Invalid loop index (hint: i=sqrt(-1)!)"));
      }
    }
    gen test=argsv[1];
    if (is_equal(test))
      test = symb_same(test._SYMBptr->feuille._VECTptr->front(),test._SYMBptr->feuille._VECTptr->back());
    // FIXME: eval local variables in test that are not in increment and prog
    gen increment=to_increment(argsv[2]);
    gen prog=argsv[3];
    if (prog.type==_SYMB && prog._SYMBptr->sommet==at_bloc)
      prog=prog._SYMBptr->feuille;
    vecteur forprog=prog.type==_VECT?*prog._VECTptr:vecteur(1,prog);
    iterateur it,itbeg=forprog.begin(),itend=forprog.end();
    for (it=itbeg;it!=itend;++it){
      *it=to_increment(equaltosto(*it,contextptr));
    }
    gen res,oldres;
    // loop
    int eval_lev=eval_level(newcontextptr);
    debug_struct * dbgptr=debug_ptr(newcontextptr);
    int & dbgptr_current_instruction = dbgptr->current_instruction;
    int save_current_instruction=dbgptr_current_instruction;
    gen testf;
#ifndef NO_STDEXCEPT
    try {
#endif
      bool findlabel=false;
      gen label,newres;
      int counter=0;
      int for_in=0; // 1 in list, 2 in string
      vecteur * for_in_v=0;
      string * for_in_s=0;
      gen index_name;
      gen gforin; 
      size_t testsize=0;
      if ((test.is_symb_of_sommet(at_for) || test.is_symb_of_sommet(at_pour))&& test._SYMBptr->feuille.type==_VECT && (testsize=test._SYMBptr->feuille._VECTptr->size())>=2){
	gen tmp=eval((*test._SYMBptr->feuille._VECTptr)[1],eval_lev,newcontextptr);
	if (testsize==3)
	  forelse=(*test._SYMBptr->feuille._VECTptr)[2];
	bool ismap=tmp.type==_MAP;
	if (ismap){
	  // copy indices
	  vecteur v;
	  gen_map::iterator it=tmp._MAPptr->begin(),itend=tmp._MAPptr->end();
	  for (;it!=itend;++it){
	    v.push_back(it->first);
	  }
	  tmp=v;
	}
	if (tmp.type==_VECT){
	  // re-store a copy of tmp because we might modify it inplace
	  // (not for maps)
	  gforin=gen(*tmp._VECTptr,tmp.subtype);
	  if (!ismap && test._SYMBptr->feuille._VECTptr->back().type==_IDNT)
	    sto(gforin,test._SYMBptr->feuille._VECTptr->back(),false,contextptr);
	  for_in_v=gforin._VECTptr;
	  for_in=1;
	}
	else {
	  if (tmp.type==_STRNG){
	    // re-store a copy of tmp because we might modify it inplace
	    gforin=string2gen(*tmp._STRNGptr,false);
	    if (test._SYMBptr->feuille._VECTptr->back().type==_IDNT)
	      sto(gforin,test._SYMBptr->feuille._VECTptr->back(),false,contextptr);
	    for_in_s=gforin._STRNGptr;
	    for_in=2;
	  }
	  else
	    return gensizeerr(contextptr);
	}
	index_name=test._SYMBptr->feuille._VECTptr->front();
      }
      // check if we have a standard for loop
      bool stdloop=(itend-it)<5 && contextptr && !for_in && is_inequation(test) && test._SYMBptr->feuille.type==_VECT && test._SYMBptr->feuille._VECTptr->size()==2 && !dbgptr->debug_mode;
      stdloop = stdloop && increment.type==_SYMB && (increment._SYMBptr->sommet==at_increment || increment._SYMBptr->sommet==at_decrement);
      gen index,stopg;
      int *idx=0,step,stop;
      if (stdloop){
	index=increment._SYMBptr->feuille;
	if (index.type==_VECT) index=index._VECTptr->front();
	stdloop=index==test._SYMBptr->feuille._VECTptr->front();
      }
      // loop initialisation
      equaltosto(initialisation,contextptr).eval(eval_lev,newcontextptr);
      if (stdloop){
	stopg=test._SYMBptr->feuille._VECTptr->back();
	gen stopindex=eval(stopg,1,contextptr);
	is_integral(stopindex);
	stop=stopindex.val;
	gen incrementstep=increment._SYMBptr->feuille;
	incrementstep=incrementstep.type==_VECT?incrementstep._VECTptr->back():1;
	is_integral(incrementstep);
	step=incrementstep.val;
	if (increment._SYMBptr->sommet==at_decrement) 
	  step=-step;
	stdloop=index.type==_IDNT && incrementstep.type==_INT_ && stopindex.type==_INT_;
      }
      if (stdloop){
	const context * cur=contextptr;
	sym_tab::iterator it=cur->tabptr->end(),itend=it;
	for (;cur->previous;cur=cur->previous){
	  it=cur->tabptr->find(index._IDNTptr->id_name);
	  itend=cur->tabptr->end();
	  if (it!=itend)
	    break;
	}
	if (it==itend){
	  it=cur->tabptr->find(index._IDNTptr->id_name);
	  itend=cur->tabptr->end();
	}
	// compute idx
	if (it!=itend && it->second.type==_INT_ 
     	    && (stop-it->second.val)/step>=5){
	  idx=&it->second.val;
	  // adjust stop for a loop with condition *idx!=stop
	  int niter=-1;
	  unary_function_ptr & u=test._SYMBptr->sommet;
	  if (u==at_inferieur_strict) {
	    if (*idx>=stop)
	      niter=0;
	    else
	      niter=step<0?-1:(stop+step-1-*idx)/step;
	  }
	  if (u==at_superieur_strict){
	    if (*idx<=stop)
	      niter=0;
	    else
	      niter=step>0?-1:(*idx-stop-step-1)/(-step);
	  }
	  if (u==at_inferieur_egal){
	    if (*idx>stop)
	      niter=0;
	    else
	      niter=step<0?-1:(stop+step-*idx)/step;
	  }
	  if (u==at_superieur_egal){
	    if (*idx<stop)
	      niter=0;
	    else
	      niter=step>0?-1:(*idx-stop-step)/(-step);
	  }
	  if (niter<0){
	    if (bound)
	      leave(protect,loop_var,newcontextptr);
	    return gensizeerr("Infinite number of iterations");
	  }
	  stop=*idx+niter*step;
	  // check that index and stopg are not modified inside the loop
	  if (!chk_forprog(forprog,index,stopg))
	    idx=0;
	}
      }
      bool oneiter=idx && (itend-itbeg==1) && !dbgptr->debug_mode;
      if (oneiter){
	for (;!interrupted && *idx!=stop;*idx+=step){
#ifdef SMARTPTR64
	  swapgen(oldres,res);
#else
	  oldres=res;
#endif
	  if (!itbeg->in_eval(eval_lev,res,newcontextptr))
	    res=*itbeg;
	  if (res.type!=_SYMB) 
	    continue;
	  if (is_return(res,newres)) {
	    if (bound)
	      leave(protect,loop_var,newcontextptr);
	    return res;
	  }
	  unary_function_ptr & u=res._SYMBptr->sommet;
	  if (u==at_break){
	    res=u; // res=oldres;
	    break;
	  }
	  if (u==at_continue){
	    res=oldres;
	    break;
	  }
	}
      }
      else {
	for (;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
	     idx?*idx!=stop:((for_in && test.val)?set_for_in(counter,for_in,for_in_v,for_in_s,index_name,newcontextptr):for_test(test,testf,eval_lev,newcontextptr));
	     ++counter,idx?*idx+=step:((test.val && increment.type)?increment.eval(eval_lev,newcontextptr).val:0)
#pragma clang diagnostic pop
	     ){
	  if (interrupted || (testf.type!=_INT_ && is_undef(testf)))
	    break;
	  dbgptr_current_instruction=save_current_instruction;
	  findlabel=false;
	  // add a test for boucle of type program/composite
	  // if that's the case call eval with test for break and continue
	  for (it=itbeg;!interrupted && it!=itend;++it){
#ifdef SMARTPTR64
	    swapgen(oldres,res);
#else
	    oldres=res;
#endif
	    ++dbgptr_current_instruction;
	    if (dbgptr->debug_mode){
	      debug_loop(res,newcontextptr);
	      if (is_undef(res)){
		increment_instruction(it+1,itend,dbgptr);
		if (bound)
		  leave(protect,loop_var,newcontextptr);
		return res;
	      }
	    }
	    if (!findlabel){
	      // res=it->eval(eval_lev,newcontextptr);
	      if (!it->in_eval(eval_lev,res,newcontextptr))
		res=*it;
	      if (res.type<=_POLY) 
		continue;
	    }
	    else {
#ifdef TIMEOUT
	      control_c();
#endif
	      if (ctrl_c || interrupted || (res.type==_STRNG && res.subtype==-1)){
		interrupted = true; ctrl_c=false;
		*logptr(contextptr) << gettext("Stopped in loop") << '\n';
		gensizeerr(gettext("Stopped by user interruption."),res);
		break;
	      }
	      res=*it;
	    }
	    if (is_return(res,newres)) {
	      increment_instruction(it+1,itend,dbgptr);
	      if (bound)
		leave(protect,loop_var,newcontextptr);
	      return res;
	    }
	    if (res.type==_SYMB){
	      unary_function_ptr & u=res._SYMBptr->sommet;
	      if (!findlabel){ 
		if (u==at_break){
		  increment_instruction(it+1,itend,dbgptr);
		  test=zero;
		  idx=0;
		  res=u; // res=oldres;
		  break;
		}
		if (u==at_continue){
		  increment_instruction(it+1,itend,dbgptr);
		  res=oldres;
		  break;
		}
	      }
	      else {
		if (u==at_label && label==res._SYMBptr->feuille)
		  findlabel=false;
	      }
	      if (!findlabel && u==at_goto){
		findlabel=true;
		label=res._SYMBptr->feuille;
	      }
	    } // end res.type==_SYMB
	    if (findlabel && it+1==itend)
	      it=itbeg-1;
	  } // end of loop of FOR bloc instructions
	} // end of user FOR loop
      } // end else one iteration
      dbgptr->current_instruction=save_current_instruction;
      increment_instruction(itbeg,itend,dbgptr);
#ifndef NO_STDEXCEPT
    } // end try
    catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      if (bound)
	leave(protect,loop_var,newcontextptr);
      return gensizeerr(e.what());
      gen res(string2gen(e.what(),false));
      res.subtype=-1;
      return res;
    }
#endif
    if (bound)
      leave(protect,loop_var,newcontextptr);
    if (is_undef(testf))
      return testf;
    if (res!=at_break && forelse!=0)
      forelse.in_eval(eval_lev,res,contextptr);
    return res==at_break?string2gen("breaked",false):res;
  }

  static const char _for_s []="for";
  static define_unary_function_eval2_index (143,__for,&_for,_for_s,&printasfor);
  define_unary_function_ptr5( at_for ,alias_at_for,&__for,_QUOTE_ARGUMENTS,0);

  // returns level or -RAND_MAX on error
  int giac_bind(const vecteur & vals_,const vecteur & vars_,context * & contextptr){
    vecteur vals(vals_),vars(vars_),vals1,vars1;
    // reorder: search in vals_ var=value and corresponding vars in vars_
    for (int i=0;i<vals_.size();++i){
      if (!vals[i].is_symb_of_sommet(at_equal))
	continue;
      gen f=vals[i]._SYMBptr->feuille;
      if (f.type!=_VECT || f._VECTptr->size()!=2 || f._VECTptr->front().type!=_IDNT)
	continue;
      gen var=f._VECTptr->front(),val=f._VECTptr->back();int j=0;
      for (;j<vars.size();++j){
	if (!vars[i].is_symb_of_sommet(at_equal))
	  continue;
	f=vars[j]._SYMBptr->feuille;
	if (f.type!=_VECT || f._VECTptr->size()!=2 || f._VECTptr->front()!=var)
	  continue;
	break;
      }
      if (j<vars.size()){
	vars1.push_back(var);
	vals1.push_back(val);
	vars.erase(vars.begin()+j);
	vals.erase(vals.begin()+i);
	--i;
      }
    }
    // add remaining
    vals=mergevecteur(vals1,vals);
    vars=mergevecteur(vars1,vars);
#if 1
    int ins=int(vals.size());
    for (int i=int(vars.size())-1;i>=0;--i){
      if (vars.size()<=vals.size())
	break;
      if (vars[i].is_symb_of_sommet(at_equal))
	vals.insert(vals.begin()+ins,eval(vars[i]._SYMBptr->feuille[1],prog_eval_level(contextptr),contextptr));
      else {
	if (ins>0)
	  --ins;
      }
    }
#else
    for (;vars.size()>vals.size();){
      int s=int(vals.size());
      if (!vars[s].is_symb_of_sommet(at_equal))
	break;
      vals.push_back(vars[s]._SYMBptr->feuille[1]);
    }
#endif
    for (int i=0;i<vars.size();++i){
      if (vars[i].is_symb_of_sommet(at_equal))
	vars[i]=vars[i]._SYMBptr->feuille[0];
    }
#ifdef KHICAS
    if (vals.size()==1 && vars.size()!=1 && vals.front().type==_VECT)
      vals=*vals.front()._VECTptr;
#endif
    if (vals.size()!=vars.size()){
#ifdef DEBUG_SUPPORT
      setsizeerr(gen(vals).print(contextptr)+ " size() != " + gen(vars).print(contextptr));
#endif
      return -RAND_MAX;
    }
    if (debug_ptr(contextptr)->debug_localvars)
      *debug_ptr(contextptr)->debug_localvars=vars;
    const_iterateur it=vals.begin(),itend=vals.end();
    const_iterateur jt=vars.begin();
    gen tmp;
    if (contextptr){
      context * newcontextptr = new context(* contextptr);
      newcontextptr->tabptr = new sym_tab;
      if (contextptr->globalcontextptr)
	newcontextptr->globalcontextptr = contextptr->globalcontextptr;
      else 
	newcontextptr->globalcontextptr = contextptr;
      newcontextptr->previous=contextptr;
      contextptr=newcontextptr;
      if (debug_ptr(contextptr))
	debug_ptr(contextptr)->debug_contextptr=contextptr;
    }
    for (;it!=itend;++it,++jt){
      if (jt->type==_SYMB){
	if (jt->_SYMBptr->sommet==at_check_type){
	  tmp=jt->_SYMBptr->feuille._VECTptr->back();
	  if (is_undef(_check_type(makevecteur(jt->_SYMBptr->feuille._VECTptr->front(),*it),contextptr)))
	    return -RAND_MAX;
	}
	else {
	  if (jt->_SYMBptr->sommet==at_double_deux_points ){
	    tmp=jt->_SYMBptr->feuille._VECTptr->front();
	    if (is_undef(_check_type(makevecteur(jt->_SYMBptr->feuille._VECTptr->back(),*it),contextptr)))
	      return -RAND_MAX;
	  }
	  else { 
	    if (jt->_SYMBptr->sommet==at_of){
	      tmp=jt->_SYMBptr->feuille._VECTptr->front();
	      *logptr(contextptr) << gettext("Invalid variable ")+jt->print(contextptr)+gettext(" using ")+tmp.print(contextptr)+gettext(" instead.");
	    }
	    else 
	      tmp=*jt;
	  }
	}
      }
      else
	tmp=*jt;
      if (tmp.type==_IDNT){
	const char * name=tmp._IDNTptr->id_name;
	int bl=strlen(name);
	gen a=*it;
	if (bl>=2 && name[bl-2]=='_' && (a.type!=_STRNG || a.subtype!=-1)){
	  switch (name[bl-1]){
	  case 'd':
	    if (a.type!=_INT_ && a.type!=_DOUBLE_ && a.type!=_FRAC){
	      *logptr(contextptr) << gettext("Unable to convert to float ")+a.print(contextptr) << '\n';
	      return -RAND_MAX;
	    }
	    break;
	  case 'f':
	    if (a.type==_FRAC)
	      break;
	  case 'i': case 'l':
	    if (a.type==_DOUBLE_ && a._DOUBLE_val<=RAND_MAX && a._DOUBLE_val>=-RAND_MAX){
	      int i=int(a._DOUBLE_val);
	      if (i!=a._DOUBLE_val)
		*logptr(contextptr) << gettext("Converting ") << a._DOUBLE_val << gettext(" to integer ") << i << '\n';
	      a=i;
	    }
	    else{
	      if (a.type!=_INT_){
		if (a.type!=_ZINT || mpz_sizeinbase(*a._ZINTptr,2)>62){
		  *logptr(contextptr) << gettext("Unable to convert to integer ")+a.print(contextptr) << '\n';
		  return -RAND_MAX;
		}
	      }
	    }
	    break;
	  case 'v':
	    if (a.type!=_VECT){
	      *logptr(contextptr) << gettext("Unable to convert to vector ")+a.print(contextptr) << '\n';
	      return -RAND_MAX;
	    }
	    break;
	  case 's':
	    if (a.type!=_STRNG)
	      a=string2gen(a.print(contextptr),false);
	  }
	}
	if (contextptr)
	  (*contextptr->tabptr)[tmp._IDNTptr->id_name]=globalize(a);
	else
	  tmp._IDNTptr->push(protection_level,globalize(a));
      }
      else {
	if (tmp.type==_FUNC){
#ifndef NO_STDEXCEPT
	  setsizeerr(gettext("Reserved word:")+tmp.print(contextptr));
#else
	  *logptr(contextptr) << gettext("Reserved word:")+tmp.print(contextptr) << '\n';
#endif
	  return -RAND_MAX;
	}
	else {
#ifndef NO_STDEXCEPT
	  setsizeerr(gettext("Not bindable")+tmp.print(contextptr));
#else
	  *logptr(contextptr) << gettext("Not bindable")+tmp.print(contextptr) << '\n';
#endif
	  return -RAND_MAX;
	}
      }
    }
    if (!contextptr)
      ++protection_level;
    return protection_level-1;
  }

  bool leave(int protect,vecteur & vars,context * & contextptr){
    iterateur it=vars.begin(),itend=vars.end(),jt,jtend;
    gen tmp;
    if (contextptr){
      if (contextptr->previous){
	context * tmpptr=contextptr;
	contextptr=contextptr->previous;
	if (debug_ptr(contextptr))
	  debug_ptr(contextptr)->debug_contextptr=contextptr;
	if (tmpptr->tabptr){
	  delete tmpptr->tabptr;
	  delete tmpptr;
	  return true;
	}
      }
      return false;
    }
    for (;it!=itend;++it){
      if (it->type==_SYMB && it->_SYMBptr->sommet==at_check_type)
	tmp=it->_SYMBptr->feuille._VECTptr->back();
      else {
	if (it->type==_SYMB && it->_SYMBptr->sommet==at_double_deux_points)
	  tmp=it->_SYMBptr->feuille._VECTptr->front();
	else
	  tmp=*it;
      }
#ifdef DEBUG_SUPPORT
      if (tmp.type!=_IDNT) setsizeerr(gettext("prog.cc/leave"));
#endif    
      if (tmp._IDNTptr->localvalue){
	jt=tmp._IDNTptr->localvalue->begin(),jtend=tmp._IDNTptr->localvalue->end();
	for (;;){
	  if (jt==jtend)
	    break;
	  --jtend;
	  --jtend;
	  if (protect>jtend->val){
	    ++jtend;
	    ++jtend;
	    break;
	  }
	}
	tmp._IDNTptr->localvalue->erase(jtend,tmp._IDNTptr->localvalue->end());
      }
    }
    protection_level=protect;
    return true;
  }
  
  static string printaslocal(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    const_iterateur it=feuille._VECTptr->begin(),itend=feuille._VECTptr->end();
    string res;
    gen local_global=*it,locals=gen2vecteur(*it),globals=vecteur(0);
    if (local_global.type==_VECT && local_global._VECTptr->size()==2){ 
      gen f=local_global._VECTptr->front(),b=local_global._VECTptr->back();
      if (f.type!=_IDNT){
	locals=gen2vecteur(f);
	globals=gen2vecteur(b);
      }
    }
    bool python=python_compat(contextptr) && !debug_ptr(contextptr)->debug_mode;
    if (python){
      int & ind=debug_ptr(contextptr)->indent_spaces;
      if (!locals._VECTptr->empty())
	res += string(ind,' ')+"# local "+printaslocalvars(locals,contextptr)+"\n";
      if (!globals._VECTptr->empty())
	res += string(ind,' ')+"global "+printaslocalvars(globals,contextptr)+"\n";
      ++it;
      if (it->is_symb_of_sommet(at_bloc)){
	if (res.size() && res[res.size()-1]=='\n') res=res.substr(0,res.size()-1);
	res += it->print(contextptr)+'\n';
      }
      else {
	if (it->type==_VECT){
	  const_iterateur jt=it->_VECTptr->begin(),jtend=it->_VECTptr->end();
	  for (;jt!=jtend;++jt)
	    res += string(ind,' ')+jt->print(contextptr)+'\n';
	}
	else
	  res += string(ind,' ')+it->print(contextptr)+'\n';
      }
      return res;
    }
    if (!locals._VECTptr->empty()){
      res += indent(contextptr);
      if (abs_calc_mode(contextptr)==38)
	res += "LOCAL ";
      else {
	if (xcas_mode(contextptr)>0){
	  if (xcas_mode(contextptr)==3)
	    res += "Local ";
	  else
	    res += "local ";
	}
	else
	  res += "{ local ";
      }
      res += printaslocalvars(locals,contextptr);
      if (xcas_mode(contextptr)!=3)
	res +=';';
    }
    if (!globals._VECTptr->empty()){
      res += indent(contextptr);
      if (abs_calc_mode(contextptr)==38)
	res += "GLOBAL ";
      else {
	if (xcas_mode(contextptr)>0){
	  if (xcas_mode(contextptr)==3)
	    res += "Global ";
	  else
	    res += "global ";
	}
	else
	  res += " global ";
      }
      res += printaslocalvars(globals,contextptr);
      if (xcas_mode(contextptr)!=3)
	res +=';';      
    }
    if (abs_calc_mode(contextptr)==38)
      res += indent(contextptr)+"BEGIN ";
    else {
      if (xcas_mode(contextptr)>0 && xcas_mode(contextptr)!=3)
	res += indent(contextptr)+"begin ";
    }
    debug_ptr(contextptr)->indent_spaces +=2;
    ++it;
    for ( ;;){
      gen tmp=*it;
      if (tmp.is_symb_of_sommet(at_bloc))
	tmp=tmp._SYMBptr->feuille;
      if (tmp.type!=_VECT)
	res += indent(contextptr)+tmp.print(contextptr);
      else {
	const_iterateur jt=tmp._VECTptr->begin(),jtend=tmp._VECTptr->end();
	for (;jt!=jtend;++jt){
	  res += indent(contextptr)+jt->print(contextptr);
	  if (xcas_mode(contextptr)!=3)
	    res += "; " ;
	}
      }
      ++it;
      if (it==itend){
	debug_ptr(contextptr)->indent_spaces -= 2;
	if (abs_calc_mode(contextptr)==38)
	  res += indent(contextptr)+"END;";
	else {
	  switch (xcas_mode(contextptr)){
	  case 0:
	    if (!locals._VECTptr->empty()) res += indent(contextptr)+"}";
	    break;
	  case 1: case 1+_DECALAGE:
	    res+=indent(contextptr)+"end;";
	    break;
	  case 2:
	    return res+=indent(contextptr)+"end_proc;";
	  }
	}
	return res;
      }
      else
	if (xcas_mode(contextptr)!=3)
	  res +="; ";
    }
  }
  gen symb_local(const gen & a,const gen & b,GIAC_CONTEXT){
    gen newa,newb;
    replace_keywords(a,b,newa,newb,contextptr);
    return symbolic(at_local,gen(makevecteur(newa,newb),_SEQ__VECT));
  }
  gen symb_local(const gen & args,GIAC_CONTEXT){
    if (args.type==_VECT && args._VECTptr->size()==2)
      return symb_local(args._VECTptr->front(),args._VECTptr->back(),contextptr);
    return symbolic(at_local,args);
  }

  gen _local(const gen & args,const context * contextptr) {
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_local(args,contextptr);
    int s=int(args._VECTptr->size());
    if (s!=2)
      return gensizeerr(gettext("Local must have 2 args"));
    // Initialization
    gen vars=args._VECTptr->front();
    if (vars.type==_VECT && vars._VECTptr->size()==2 && vars._VECTptr->front().type!=_IDNT)
      vars = vars._VECTptr->front();
    if (vars.type!=_VECT)
      vars=makevecteur(vars);
    vecteur names,values;
    iterateur it=vars._VECTptr->begin(),itend=vars._VECTptr->end();
    names.reserve(itend-it);
    values.reserve(itend-it);
    for (;it!=itend;++it){
      if (it->type==_IDNT){
	names.push_back(*it);
#if 1
	gen err=string2gen(gettext("Uninitialized local variable ")+it->print(contextptr),false);
	err.subtype=-1;
	values.push_back(err);
#else
	values.push_back(0);
#endif
	continue;
      }
      if ( (it->type!=_SYMB) || (it->_SYMBptr->sommet!=at_sto && it->_SYMBptr->sommet!=at_equal))
	return gentypeerr(contextptr);
      gen nom=it->_SYMBptr->feuille._VECTptr->back();
      gen val=it->_SYMBptr->feuille._VECTptr->front();
      if (it->_SYMBptr->sommet==at_equal)
	swapgen(nom,val);
      val=val.eval(eval_level(contextptr),contextptr);
      if (nom.type!=_IDNT)
	return gentypeerr(contextptr);
      names.push_back(nom);
      values.push_back(val);
    }
    context * newcontextptr = (context *) contextptr;
    int protect=giac_bind(values,names,newcontextptr);
    gen prog=args._VECTptr->back(),res,newres;
    if (protect!=-RAND_MAX){
      if (prog.type!=_VECT){
	prog=equaltosto(prog,contextptr);
	++debug_ptr(newcontextptr)->current_instruction;
	if (debug_ptr(newcontextptr)->debug_mode){
	  debug_loop(res,newcontextptr);
	  if (!is_undef(res)){
	    if (!prog.in_eval(eval_level(newcontextptr),res,newcontextptr))
	      res=prog;
	  }
	}
	else {
	  if (!prog.in_eval(eval_level(newcontextptr),res,newcontextptr))
	    res=prog;
	}
      }
      else {
	it=prog._VECTptr->begin(),itend=prog._VECTptr->end();
	bool findlabel=false;
	gen label;
	for (;!ctrl_c && !interrupted && it!=itend;++it){
#ifdef TIMEOUT
	  control_c();
#endif
	  ++debug_ptr(newcontextptr)->current_instruction;
	  // cout << *it << '\n';
	  if (debug_ptr(newcontextptr)->debug_mode){
	    debug_loop(res,newcontextptr);
	    if (!is_undef(res)){
	      if (!findlabel){
		if (!equaltosto(*it,contextptr).in_eval(eval_level(newcontextptr),res,newcontextptr))
		  res=*it;
	      }
	      else
		res=*it;
	    }
	  }
	  else {
	    if (!findlabel){
	      if (!equaltosto(*it,contextptr).in_eval(eval_level(newcontextptr),res,newcontextptr))
		res=*it;
	    }
	    else
	      res=*it;
	  }
	  if (res.type==_SYMB){
	    unary_function_ptr & u=res._SYMBptr->sommet;
	    if (findlabel && u==at_label && label==res._SYMBptr->feuille)
	      findlabel=false;
	    if (!findlabel && u==at_goto){
	      findlabel=true;
	      label=res._SYMBptr->feuille;
	    }
	  }
	  if (findlabel && it+1==itend)
	    it=prog._VECTptr->begin()-1;
	  if (!findlabel && is_return(res,newres) ){
	    // res=newres;
	    break;
	  }
	}
      }
      leave(protect,names,newcontextptr);
    }
    else
      return gensizeerr(contextptr);
    return res;
  }

  static const char _local_s []="local";
  static define_unary_function_eval2_index (85,__local,&_local,_local_s,&printaslocal);
  define_unary_function_ptr5( at_local ,alias_at_local,&__local,_QUOTE_ARGUMENTS,0);

  static string printasreturn(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( abs_calc_mode(contextptr)==38 || (xcas_mode(contextptr)==1) || (xcas_mode(contextptr)==1+_DECALAGE) )
      return "RETURN("+feuille.print(contextptr)+")";
    if (xcas_mode(contextptr)==3)
      return "Return "+feuille.print(contextptr);
    return sommetstr+("("+feuille.print(contextptr)+")");
  }
  static gen symb_return(const gen & args,GIAC_CONTEXT){
    return symbolic(at_return,args);
  }
  static const char _return_s []="return";
  static define_unary_function_eval2_index (86,__return,&symb_return,_return_s,&printasreturn);
  define_unary_function_ptr( at_return ,alias_at_return ,&__return);

  static string printastry_catch(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()<3) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    const_iterateur it=feuille._VECTptr->begin();//,itend=feuille._VECTptr->end();
    string res;
    if (feuille._VECTptr->size()==4){
      res = "IFERR ";
      res += printasinnerbloc(*it,contextptr);
      ++it;
      ++it;
      res += " THEN ";
      res += printasinnerbloc(*it,contextptr);
      ++it;
      res += " ELSE ";
      res += printasinnerbloc(*it,contextptr);
      res += " END";
      return res;
    }
    if (xcas_mode(contextptr)==3)
      res += "Try";
    else
      res += "try ";
    res += it->print(contextptr);
    ++it;
    if (xcas_mode(contextptr)==3){
      res += indent(contextptr)+"Else";
      ++it;
      if (!is_undef(*it))
	res += printasinnerbloc(*it,contextptr);
      res += indent(contextptr)+"EndTry";
    }
    else {
      if (res[res.size()-1]!='}')
	res += "; ";
      res += "catch(" + it->print(contextptr) + ")";
      ++it;
      res += it->print(contextptr);
      if (res[res.size()-1]!='}')
	res += "; ";
    }
    return res;
  }
  
  gen symb_try_catch(const gen & args){
    return symbolic(at_try_catch,args);
  }
  gen _try_catch(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_try_catch(args);
    int args_size=int(args._VECTptr->size());
    if (args_size!=3 && args_size!=4)
      return gensizeerr(gettext("Try_catch must have 3 or 4 args"));
    gen res;
    int saveprotect=protection_level;
    vector< vector<int> > save_sst_at_stack(debug_ptr(contextptr)->sst_at_stack);
    vecteur save_args_stack(debug_ptr(contextptr)->args_stack);
    vector<int> save_current_instruction_stack=debug_ptr(contextptr)->current_instruction_stack;
    int save_current_instruction=debug_ptr(contextptr)->current_instruction;
    bool do_else_try=args_size==4;
#ifndef NO_STDEXCEPT
    try {
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode)
	debug_loop(res,contextptr);
      res=args._VECTptr->front().eval(eval_level(contextptr),contextptr);
    }
    catch (std::runtime_error & error ){
      last_evaled_argptr(contextptr)=NULL;
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode)
	debug_loop(res,contextptr);
      // ??? res=args._VECTptr->front().eval(eval_level(contextptr),contextptr);
      do_else_try=false;
      if (!contextptr)
	protection_level=saveprotect;
      debug_ptr(contextptr)->sst_at_stack=save_sst_at_stack;
      debug_ptr(contextptr)->args_stack=save_args_stack;
      debug_ptr(contextptr)->current_instruction_stack=save_current_instruction_stack;
      gen id=(*(args._VECTptr))[1];
      string er(error.what());
      er = '"'+er+'"'; // FIXME? string2gen(er,false) instead?
      gen tmpsto;
      if (id.type==_IDNT)
	tmpsto=sto(gen(er,contextptr),id,contextptr);
      if (is_undef(tmpsto)) return tmpsto;
      debug_ptr(contextptr)->current_instruction=save_current_instruction;
      increment_instruction(args._VECTptr->front(),contextptr);
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode)
	debug_loop(res,contextptr);
      res=(*args._VECTptr)[2].eval(eval_level(contextptr),contextptr);
    }
#else
    ++debug_ptr(contextptr)->current_instruction;
    if (debug_ptr(contextptr)->debug_mode)
      debug_loop(res,contextptr);
    if (is_undef(res)) return res;
    res=args._VECTptr->front().eval(eval_level(contextptr),contextptr);
    if (is_undef(res)){
      do_else_try=false;
      if (!contextptr)
	protection_level=saveprotect;
      debug_ptr(contextptr)->sst_at_stack=save_sst_at_stack;
      debug_ptr(contextptr)->args_stack=save_args_stack;
      debug_ptr(contextptr)->current_instruction_stack=save_current_instruction_stack;
      gen id=(*(args._VECTptr))[1];
      string er(gen2string(res));
      er = '"'+er+'"';
      gen tmpsto;
      if (id.type==_IDNT)
	tmpsto=sto(gen(er,contextptr),id,contextptr);
      if (is_undef(tmpsto)) return tmpsto;
      debug_ptr(contextptr)->current_instruction=save_current_instruction;
      increment_instruction(args._VECTptr->front(),contextptr);
      ++debug_ptr(contextptr)->current_instruction;
      if (debug_ptr(contextptr)->debug_mode){
	debug_loop(res,contextptr);
	if (is_undef(res)) return res;
      }
      res=(*args._VECTptr)[2].eval(eval_level(contextptr),contextptr);
    }
#endif
    if (do_else_try){
      if ((res.type==_SYMB && res._SYMBptr->sommet==at_return) || (res.type==_FUNC && (res==at_return || res==at_break || res==at_continue)))
	;
      else
	res=args._VECTptr->back().eval(eval_level(contextptr),contextptr);
    }
    debug_ptr(contextptr)->current_instruction=save_current_instruction;
    increment_instruction(args._VECTptr->front(),contextptr);
    increment_instruction(args._VECTptr->back(),contextptr);
    return res;
  }
  static const char _try_catch_s []="try_catch";
  static define_unary_function_eval2_index (177,__try_catch,&_try_catch,_try_catch_s,&printastry_catch);
  define_unary_function_ptr5( at_try_catch ,alias_at_try_catch,&__try_catch,_QUOTE_ARGUMENTS,0);

  static gen feuille_(const gen & g,const gen & interval,GIAC_CONTEXT){
    vecteur v;
    if (g.type==_SYMB){
      gen & f=g._SYMBptr->feuille;
      if (f.type==_VECT)
	v=*f._VECTptr;
      else
	v=vecteur(1,f);
    }
    else {
      if (g.type==_VECT)
	v=*g._VECTptr;
      else
	v=vecteur(1,g);
    }
    int s=int(v.size());
    if (interval.type==_INT_){
      int i=interval.val-array_start(contextptr); //-(xcas_mode(contextptr)!=0);
      if (i==-1 && g.type==_SYMB)
	return g._SYMBptr->sommet;
      if (i<0 || i>=s)
	return gendimerr(contextptr);
      return v[i];
    }
    if (interval.is_symb_of_sommet(at_interval)&& interval._SYMBptr->feuille.type==_VECT){
      vecteur & w=*interval._SYMBptr->feuille._VECTptr;
      if (w.size()!=2 || w.front().type!=_INT_ || w.back().type!=_INT_)
	return gentypeerr(contextptr);
      int i=w.front().val,j=w.back().val;
      if (i>j)
	return gen(vecteur(0),_SEQ__VECT);
      if (array_start(contextptr)){
	--i;
	--j;
      }
      if (i<0 || i>=s || j<0 || j>=s)
	return gendimerr(contextptr);
      return gen(vecteur(v.begin()+i,v.begin()+j+1),_SEQ__VECT);
    }
    return gensizeerr(contextptr);
  }
  gen _feuille(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT){
      if (args.subtype==_SEQ__VECT && args._VECTptr->size()==2)
	return feuille_(args._VECTptr->front(),args._VECTptr->back(),contextptr);
      return gen(*args._VECTptr,_SEQ__VECT);
    }
    if (args.type==_MAP){
      const gen_map & m=*args._MAPptr;
      gen_map::const_iterator it=m.begin(),itend=m.end();
      vecteur res;
      for (;it!=itend;++it){
	res.push_back(makevecteur(it->first,it->second));
      }
      return res;
    }
    if (args.type!=_SYMB)
      return args;
    gen tmp=args._SYMBptr->feuille;
    if (tmp.type==_VECT)
      tmp.subtype=_SEQ__VECT;
    return tmp;
  }
  static const char _feuille_s []="op";
  static define_unary_function_eval2 (__feuille,&_feuille,_feuille_s,&printassubs);
  define_unary_function_ptr5( at_feuille ,alias_at_feuille,&__feuille,0,true);
  
  gen _maple_op(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT){
      vecteur & v=*args._VECTptr;
      if (args.subtype==_SEQ__VECT && v.size()>1)
	return feuille_(v.back(),v.front(),contextptr);
      return gen(v,_SEQ__VECT);
    }
    if (args.type!=_SYMB)
      return args; // was symbolic(at_maple_op,args);
    return args._SYMBptr->feuille;
  }
  static const char _maple_op_s []="op";
  static define_unary_function_eval2 (__maple_op,&_maple_op,_maple_op_s,&printasmaple_subs);
  define_unary_function_ptr( at_maple_op ,alias_at_maple_op ,&__maple_op);
  
  gen _sommet(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_SYMB)
      return at_id;
    int nargs;
    if (args._SYMBptr->feuille.type==_VECT)
        nargs=int(args._SYMBptr->feuille._VECTptr->size());
    else
        nargs=1;
    return gen(args._SYMBptr->sommet,nargs);
  }
  static const char _sommet_s []="sommet";
  static define_unary_function_eval (__sommet,&_sommet,_sommet_s);
  define_unary_function_ptr5( at_sommet ,alias_at_sommet,&__sommet,0,true);

  // replace in g using equalities in v
  gen subsop(const vecteur & g,const vecteur & v,const gen & sommet,GIAC_CONTEXT){
    if (v.size()==2 && !v[0].is_symb_of_sommet(at_equal))
      return subsop(g,vecteur(1,symb_equal(v[0],v[1])),sommet,contextptr);
    gen newsommet=sommet;
    vecteur res(g);
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if ( (!is_equal(*it) && !it->is_symb_of_sommet(at_same)) || it->_SYMBptr->feuille.type!=_VECT || it->_SYMBptr->feuille._VECTptr->size()!=2){
	*logptr(contextptr) << gettext("Unknown subsop rule ") << *it << '\n';
	continue;
      }
      vecteur w=*it->_SYMBptr->feuille._VECTptr;
      if (w.front().type==_VECT){
	vecteur rec=*w.front()._VECTptr;
	if (rec.size()<1)
	  return gendimerr(contextptr);
	if (rec.size()==1)
	  w.front()=rec.front();
	else {
	  int i=rec.front().val;
	  if (i>=0 && array_start(contextptr))
	    --i;
	  if (i<0)
	    i += int(res.size());
	  if (rec.front().type!=_INT_ || i<0 || i>=signed(res.size()))
	    return gendimerr(contextptr);
	  if (is_undef( (res[i]=subsop(res[i],vecteur(1,symbolic(at_equal,gen(makevecteur(vecteur(rec.begin()+1,rec.end()),w.back()),_SEQ__VECT))),contextptr)) ) )
	    return res[i];
	  continue;
	}
      }
      if (w.front().type!=_INT_)
	continue;
      int i=w.front().val;
      if (i>=0 && array_start(contextptr))
	--i;
      /*
      if (i==-1){
	newsommet=w.back();
	continue;
      }
      */
      if (i<0)
	i += int(res.size());
      if (i<0 || i>=signed(res.size()))
	return gendimerr(contextptr);
      res[i]=w.back();
    }
    it=res.begin();
    itend=res.end();
    vecteur res1;
    res1.reserve(itend-it);
    for (;it!=itend;++it){
      if (it->type!=_VECT || it->subtype!=_SEQ__VECT || !it->_VECTptr->empty() )
	res1.push_back(*it);
    }
    if (newsommet.type!=_FUNC)
      return res1;
    else
      return symbolic(*newsommet._FUNCptr,res1);
  }
  gen subsop(const gen & g,const vecteur & v,GIAC_CONTEXT){
    if (g.type==_VECT)
      return subsop(*g._VECTptr,v,0,contextptr);
    if (g.type!=_SYMB)
      return g;
    vecteur w(gen2vecteur(g._SYMBptr->feuille));
    return subsop(w,v,g._SYMBptr->sommet,contextptr);
  }
  gen _maple_subsop(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gendimerr(contextptr);
    return subsop(v.back(),vecteur(v.begin(),v.end()-1),contextptr);
  }
  static const char _maple_subsop_s []="subsop";
  static define_unary_function_eval2 (__maple_subsop,&_maple_subsop,_maple_subsop_s,&printasmaple_subs);
  define_unary_function_ptr( at_maple_subsop ,alias_at_maple_subsop ,&__maple_subsop);
  
  gen _subsop(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gendimerr(contextptr);
    return subsop(v.front(),vecteur(v.begin()+1,v.end()),contextptr);
  }
  static const char _subsop_s []="subsop";
  static define_unary_function_eval2 (__subsop,&_subsop,_subsop_s,&printassubs);
  define_unary_function_ptr( at_subsop ,alias_at_subsop ,&__subsop);
  
  // static gen symb_append(const gen & args){  return symbolic(at_append,args);  }
  gen _append(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( args.type!=_VECT || !args._VECTptr->size() )
      return gensizeerr(contextptr);
    const_iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (itend-it==2 && it->type==_STRNG && (it+1)->type==_STRNG)
      return string2gen(*it->_STRNGptr+*(it+1)->_STRNGptr,false);
    if (it->type!=_VECT)
      return gensizeerr(contextptr);
    vecteur v(*it->_VECTptr);
    int subtype=it->subtype;
    ++it;
    if (v.size()+(itend-it)>LIST_SIZE_LIMIT)
      return gendimerr(contextptr);
    for (;it!=itend;++it)
      v.push_back(*it);
    return gen(v,subtype);
  }
  static const char _append_s []="append";
  static define_unary_function_eval (__append,&_append,_append_s);
  define_unary_function_ptr5( at_append ,alias_at_append,&__append,0,true);

  // static gen symb_prepend(const gen & args){  return symbolic(at_prepend,args); }
  gen _prepend(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_STRNG && args._VECTptr->back().type==_STRNG)
      return args._VECTptr->back()+args._VECTptr->front();
    if ( (args.type!=_VECT) || (!args._VECTptr->size()) || (args._VECTptr->front().type!=_VECT) )
      return gensizeerr(contextptr);
    gen debut=args._VECTptr->front();
    return gen(mergevecteur(cdr_VECT(*args._VECTptr),*debut._VECTptr),debut.subtype);
  }
  static const char _prepend_s []="prepend";
  static define_unary_function_eval (__prepend,&_prepend,_prepend_s);
  define_unary_function_ptr5( at_prepend ,alias_at_prepend,&__prepend,0,true);

  static bool compare_VECT(const vecteur & v,const vecteur & w){
    int s1=int(v.size()),s2=int(w.size());
    if (s1!=s2)
      return s1<s2;
    const_iterateur it=v.begin(),itend=v.end(),jt=w.begin();
    for (;it!=itend;++it,++jt){
      if (*it!=*jt){
	return set_sort(*it,*jt);
      }
    }
    // setsizeerr(); should not happen... commented because it happens!
    return false;
  }

  // return true if *this is "strictly less complex" than other
  bool set_sort(const gen & t,const gen & other ) {
    if ( (t==minus_inf || other==plus_inf) && t!=other)
      return true;
    if ( (t==plus_inf || other==minus_inf) && t!=other)
      return false;
    if (t.type!=other.type){
      gen t1=evalf_double(t,1,context0),o1=evalf_double(other,1,context0);
      if (t1.type==_DOUBLE_ && o1.type==_DOUBLE_)
        return t1._DOUBLE_val<o1._DOUBLE_val;
      return t.type < other.type;
    }
    if (t==other)
      return false;
    switch (t.type) {
    case _INT_:
      return t.val<other.val;
    case _ZINT:
      return is_greater(other,t,context0);
    case _DOUBLE_: case _FLOAT_: case _REAL:
      return is_greater(other,t,context0);
    case _CPLX: 
      if (*t._CPLXptr==*other._CPLXptr)
        return set_sort(*(t._CPLXptr+1),*(other._CPLXptr+1));
      return set_sort(*(t._CPLXptr),*(other._CPLXptr));
    case _IDNT:
      return strcmp(t._IDNTptr->id_name,other._IDNTptr->id_name)<0;
    case _POLY:
      if (t._POLYptr->coord.size()!=other._POLYptr->coord.size())
	return t._POLYptr->coord.size()<other._POLYptr->coord.size();
      return set_sort(t._POLYptr->coord.front().value,other._POLYptr->coord.front().value);
    case _MOD:
      if (*(t._MODptr+1)!=*(other._MODptr+1)){
#ifndef NO_STDEXCEPT
	setsizeerr(gettext("islesscomplexthan mod"));
#endif
      }
      return set_sort(*t._MODptr,*other._MODptr);
    case _SYMB:
      if (t._SYMBptr->sommet!=other._SYMBptr->sommet ){
#ifdef GIAC_HAS_STO_38 // otherwise 1 test of chk_xavier fails, needs to check
	int c=strcmp(t._SYMBptr->sommet.ptr()->s,other._SYMBptr->sommet.ptr()->s);
	if (c) return c<0;
#endif 
	return (alias_type) t._SYMBptr->sommet.ptr() <(alias_type) other._SYMBptr->sommet.ptr();
      }
      return set_sort(t._SYMBptr->feuille,other._SYMBptr->feuille);
      // return false;
    case _VECT:
      return compare_VECT(*t._VECTptr,*other._VECTptr);
    case _EXT:
      if (*(t._EXTptr+1)!=*(other._EXTptr+1))
	return set_sort(*(t._EXTptr+1),*(other._EXTptr+1));
      return set_sort(*t._EXTptr,*other._EXTptr);
    case _STRNG:
      return *t._STRNGptr<*other._STRNGptr;
    default:
      return t.print(context0)<other.print(context0); 
    }
  }

  bool interval2realset(gen & g,bool lopen=false,bool ropen=false){
    if (!g.is_symb_of_sommet(at_interval))
      return false;
    gen tmp=gen(vecteur(1,gen(gen2vecteur(g._SYMBptr->feuille),_LINE__VECT)),_SET__VECT);
    vecteur excl;
    if (lopen)
      excl.push_back(g._SYMBptr->feuille[0]);
    if (ropen)
      excl.push_back(g._SYMBptr->feuille[1]);
    g=gen(makevecteur(tmp,excl),_REALSET__VECT);
    return true;
  }
  // convert list of real to realset
  bool set2realset(const vecteur & v_,vecteur & w,GIAC_CONTEXT){
    vecteur v(v_),wint;
    chk_set(v);
    for (int i=0;i<v.size();++i){
      gen cur=v[i];
      gen g=evalf_double(cur,1,contextptr);
      if (g.type!=_DOUBLE_)
        return false;
      wint.push_back(gen(makevecteur(cur,cur),_LINE__VECT));
    }
    //w=makevecteur(vecteur(0),gen(wint,_SET__VECT),gen(vecteur(0),_SET__VECT));
    w=makevecteur(gen(wint,_SET__VECT),gen(vecteur(0),_SET__VECT));
    return true;
  }

  gen vecteur2realset(vecteur & v,GIAC_CONTEXT){
    int s=v.size();
    if ((s!=2 && s!=3) || v[s-1].type!=_VECT || v[s-2].type!=_VECT || v[s-2]._VECTptr->empty())
      return gensizeerr(gettext("Unable to convert to realset"));
    vecteur v1(*v[s-2]._VECTptr),v2(*v[s-1]._VECTptr);
    chk_set(v1);
    chk_set(v2);
    // glue intervals in v1 if possible
    gen MA=v1[0][1];
    for (int i=1;i<v1.size();++i){
      if (v1[i][0]==MA && !binary_search(v2.begin(),v2.end(),MA,set_sort)){
        v1[i-1]=gen(makevecteur(v1[i-1][0],v1[i][1]),_LINE__VECT);
        v1.erase(v1.begin()+i);
        --i;
      }
      MA=v1[i][1];
    }
    // split v[1] intervals if necessary (excluded values inside an interval)
    int j=0; // position in v1
    gen m=v1[j][0],M=v1[j][1];
    for (int i=0;i<v2.size();++i){
      gen cur=v2[i];
      for (;j<v1.size();){
        if (is_greater(m,cur,contextptr))
          break;
        if (is_strictly_greater(M,cur,contextptr)){
          v1[j]=gen(makevecteur(m,cur),_LINE__VECT);
          v1.insert(v1.begin()+j,gen(makevecteur(m,cur),_LINE__VECT));
          ++j;
          break;
        }
        ++j;
      }
    }
    v[s-2]=v1; v[s-1]=v2;
    return gen(v,_REALSET__VECT);
  }

  bool convert_realset(gen & a,GIAC_CONTEXT){
    if (a.type!=_VECT)
      return false;
    if (a.subtype!=_REALSET__VECT){
      vecteur tmp;
      if (!set2realset(*a._VECTptr,tmp,contextptr))
        return false;
      a=gen(tmp,_REALSET__VECT);
    }
    return true;
  }
  
  gen _contains(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (a.type==_STRNG && b.type==_STRNG){
      int pos=a._STRNGptr->find(*b._STRNGptr);
      if (pos<0 || pos>=a._STRNGptr->size())
	return 0;
      else
	return pos+1;
    }
    if (a.type!=_VECT){
      if (a.type==_REAL)
	return contains(a,b);
      if (b==cst_i)
	return has_i(a);
      return gensizeerr(contextptr);
    }
    if (a.subtype==_REALSET__VECT){
      if (b.type==_VECT)
        return gensizeerr(contextptr);
      vecteur tmp(makevecteur(makevecteur(makevecteur(b,b)),vecteur(0)));
      return realset_in(tmp,*a._VECTptr,contextptr);
    }
    return equalposcomp(*a._VECTptr,b);
  }
  static const char _contains_s []="contains";
  static define_unary_function_eval (__contains,&_contains,_contains_s);
  define_unary_function_ptr5( at_contains ,alias_at_contains,&__contains,0,true);

  // check if a set A is included in a set B
  gen _is_included(const gen & args,GIAC_CONTEXT){
    if (args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen A(args._VECTptr->front()),B(args._VECTptr->back());
    if (1 || A.type!=_VECT || B.type!=_VECT) {
      int i=set_compare(A,B,contextptr);
      if (i==-2) 
        return 0; 
      if (i<0)
        return gensizeerr(contextptr);
      if (i==0 || i==2)
        return 1;
      return 0;
    }
    vecteur a(*A._VECTptr);
    vecteur b(*B._VECTptr);
    sort(b.begin(),b.end(),set_sort);
    for (unsigned i=0;i<a.size();++i){
      if (!binary_search(b.begin(),b.end(),a[i],set_sort))
	return 0;
    }
    return 1;
  }
  static const char _is_included_s []="is_included";
  static define_unary_function_eval (__is_included,&_is_included,_is_included_s);
  define_unary_function_ptr5( at_is_included ,alias_at_is_included,&__is_included,0,true);

  static gen symb_select(const gen & args){
    return symbolic(at_select,args);
  }
  static gen symb_remove(const gen & args){
    return symbolic(at_remove,args);
  }
  static gen select_remove(const gen & args,bool selecting,const context * contextptr){
    if ( (args.type!=_VECT) || (args._VECTptr->size()<2)){
      if (selecting)
	return symb_select(args);
      else
	return symb_remove(args);
    }
    gen v((*(args._VECTptr))[1]);
    int subtype;
    gen fn=0;
    if (v.type==_SYMB){
      if (v._SYMBptr->feuille.type==_VECT)
	v=v._SYMBptr->feuille;
      else
	v=makevecteur(v._SYMBptr->feuille);
      subtype=-1;
      fn=v;
    }
    else
      subtype=v.subtype;
    gen f(args._VECTptr->front());
    if ( (v.type!=_VECT) && (v.type!=_SYMB)){
      if (selecting)
	return symb_select(args);
      else {
	if (f.type==_VECT){ // remove 1st occurrence of v
	  vecteur w=*f._VECTptr;
	  for (unsigned i=0;i<w.size();++i){
	    if (w[i]==v){
	      w.erase(w.begin()+i);
	      return gen(w,f.subtype);
	    }
	  }
	  return gensizeerr(contextptr);
	}
	return symb_remove(args);
      }
    }
    bool prog=f.is_symb_of_sommet(at_program);
    vecteur otherargs(args._VECTptr->begin()+1,args._VECTptr->end());
    const_iterateur it=v._VECTptr->begin(),itend=v._VECTptr->end();
    vecteur res;
    res.reserve(itend-it);
    if (otherargs.size()==1){
      for (;it!=itend;++it){
	if (prog){
	  if (is_zero(f(*it,contextptr))!=selecting)
	    res.push_back(*it);
	}
	else {
	  if ((*it==f)==selecting )
	    res.push_back(*it);
	}
      }
    }
    else {
      for (;it!=itend;++it){
	otherargs.front()=*it;
	if (is_zero(f(otherargs,contextptr))!=selecting)
	  res.push_back(*it);
      }
    }
    if (subtype<0 && fn.type==_SYMB)
      return symbolic(fn._SYMBptr->sommet,res);
    else
      return gen(res,subtype);
  }
  gen _select(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return select_remove(args,1,contextptr);
  }
  static const char _select_s []="select";
  static define_unary_function_eval (__select,&_select,_select_s);
  define_unary_function_ptr5( at_select ,alias_at_select,&__select,0,true);

  static const char _filter_s []="filter";
  static define_unary_function_eval (__filter,&_select,_filter_s);
  define_unary_function_ptr5( at_filter ,alias_at_filter,&__filter,0,true);

  gen _remove(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return select_remove(args,0,contextptr);
  }
  static const char _remove_s []="remove";
  static define_unary_function_eval (__remove,&_remove,_remove_s);
  define_unary_function_ptr5( at_remove ,alias_at_remove,&__remove,0,true);

  static string printnostring(const gen & g,GIAC_CONTEXT){
    if (g.type==_STRNG)
      return *g._STRNGptr;
    else
      return g.print(contextptr);
  }
  static gen symb_concat(const gen & args){
    return symbolic(at_concat,args);
  }
  gen concat(const gen & g,bool glue_lines,GIAC_CONTEXT){
    if (g.type!=_VECT)
      return symb_concat(g);
    vecteur & v=*g._VECTptr;
    if (v.size()>2){
      gen h=concat(makesequence(v[0],v[1]),glue_lines,contextptr);
      for (unsigned i=2;i<v.size();++i){
	h=concat(makesequence(h,v[i]),glue_lines,contextptr);
      }
      return h;
    }
    if (v.size()!=2){
      if (g.subtype==_SEQ__VECT)
	return g;
      return symb_concat(g);
    }
    gen v0=v[0],v1=v[1];
    if (v0.type==_VECT && v1.type==_VECT){
      if (!glue_lines && v1.subtype!=_SEQ__VECT && ckmatrix(v0) && ckmatrix(v1) && v0._VECTptr->size()==v1._VECTptr->size() )
	return gen(mtran(mergevecteur(mtran(*v0._VECTptr),mtran(*v1._VECTptr))));
      else
	return gen(mergevecteur(*v0._VECTptr,*v1._VECTptr),v0.subtype);
    }
    if (v0.type==_VECT)
      return gen(mergevecteur(*v0._VECTptr,vecteur(1,v1)),v0.subtype);
    if (v1.type==_VECT)
      return gen(mergevecteur(vecteur(1,v0),*v1._VECTptr),v1.subtype);
    if ( (v0.type==_STRNG) || (v1.type==_STRNG) )
      return string2gen(printnostring(v0,contextptr) + printnostring(v1,contextptr),false);
    return 0;
  }
  gen _concat(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return concat(args,false,contextptr);
  }
  static const char _concat_s []="concat";
  static define_unary_function_eval (__concat,&_concat,_concat_s);
  define_unary_function_ptr5( at_concat ,alias_at_concat,&__concat,0,true);

  gen _extend(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2 || args._VECTptr->front().type!=_VECT || args._VECTptr->back().type!=_VECT)
      return gensizeerr(contextptr);
    return gen(mergevecteur(*args._VECTptr->front()._VECTptr,*args._VECTptr->back()._VECTptr),args._VECTptr->front().subtype);
  }
  static const char _extend_s []="extend";
  static define_unary_function_eval (__extend,&_extend,_extend_s);
  define_unary_function_ptr5( at_extend ,alias_at_extend,&__extend,0,true);

  static gen symb_option(const gen & args){
    return symbolic(at_option,args);
  }
  gen _option(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symb_option(args);
  }
  static const char _option_s []="option";
  static define_unary_function_eval (__option,&_option,_option_s);
  define_unary_function_ptr( at_option ,alias_at_option ,&__option);

  static string printascase(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) || (feuille._VECTptr->back().type!=_VECT))
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    string res("switch (");
    res += feuille._VECTptr->front().print(contextptr);
    res += "){";
    debug_ptr(contextptr)->indent_spaces +=2;
    const_iterateur it=feuille._VECTptr->back()._VECTptr->begin(),itend=feuille._VECTptr->back()._VECTptr->end();
    for (;it!=itend;++it){
      ++it;
      if (it==itend){
	res += indent(contextptr)+"default:";
	--it;
	debug_ptr(contextptr)->indent_spaces += 2;
	res += indent(contextptr)+it->print(contextptr);
	debug_ptr(contextptr)->indent_spaces -= 2;
	break;
      }
      res += indent(contextptr)+"case "+(it-1)->print(contextptr)+":";
      debug_ptr(contextptr)->indent_spaces += 2;
      res += indent(contextptr)+it->print(contextptr);
      debug_ptr(contextptr)->indent_spaces -=2;
    }
    debug_ptr(contextptr)->indent_spaces -=2;
    res+=indent(contextptr)+"}";
    return res;
  }
  gen symb_case(const gen & args){
    return symbolic(at_case,args);
  }
  gen symb_case(const gen & a,const gen & b){
    return symbolic(at_case,gen(makevecteur(a,b),_SEQ__VECT));
  }
  gen _case(const gen & args,GIAC_CONTEXT){ // FIXME DEBUGGER
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) || (args._VECTptr->back().type!=_VECT) )
      return symb_case(args);
    gen expr=args._VECTptr->front().eval(eval_level(contextptr),contextptr),res=undef,oldres,newres;
    const_iterateur it=args._VECTptr->back()._VECTptr->begin(),itend=args._VECTptr->back()._VECTptr->end();
    for (;it!=itend;){
      if (it+1==itend){
	res=it->eval(eval_level(contextptr),contextptr);
	break;
      }
      if (expr==it->eval(eval_level(contextptr),contextptr)){
	++it;
	oldres=res;
	res=it->eval(eval_level(contextptr),contextptr);
	if (res==symbolic(at_break,zero)){
	  res=oldres;
	  break;
	}
	if (res.is_symb_of_sommet(at_return))
	  break;
      }
      else
	++it;
      if (it!=itend)
	++it;
    }
    return res;
  }
  static const char _case_s []="case";
  static define_unary_function_eval2_index (123,__case,&_case,_case_s,&printascase);
  define_unary_function_ptr5( at_case ,alias_at_case,&__case,_QUOTE_ARGUMENTS,0);

  static gen symb_rand(const gen & args){
    return symbolic(at_rand,args);
  }
  static gen rand_integer_interval(const gen & x1,const gen & x2,GIAC_CONTEXT){
    gen x2x1=x2-x1;
    if (!is_positive(x2x1,contextptr))
      return rand_integer_interval(x2,x1,contextptr);
    int n=x2x1.bindigits()/gen(rand_max2).bindigits()+1;
    gen res=zero;
#ifndef USE_GMP_REPLACEMENTS
    if (unsigned(rand_max2)==(1u<<31)-1){
      mpz_t tmp;
      mpz_init(tmp);
      for (int i=0;i<n;++i){
	mpz_mul_2exp(tmp,tmp,31);
	mpz_add_ui(tmp,tmp,giac_rand(contextptr));
      }
      if (x2x1.type==_INT_)
	mpz_mul_si(tmp,tmp,x2x1.val);
      else
	mpz_mul(tmp,tmp,*x2x1._ZINTptr);
      mpz_tdiv_q_2exp(tmp,tmp,31*n);
      if (x1.type==_INT_){
	if (x1.val>0) mpz_add_ui(tmp,tmp,x1.val); else mpz_sub_ui(tmp,tmp,-x1.val);
      }
      else
	mpz_add(tmp,tmp,*x1._ZINTptr);
      res=tmp;
      mpz_clear(tmp);
      return res;
    }
#endif
#ifdef FXCG
    gen rand_max_plus_one=gen(rand_max2)+1;
#else
    static gen rand_max_plus_one=gen(rand_max2)+1;
#endif
    // Make n random numbers
    for (int i=0;i<n;++i)
      res=rand_max_plus_one*res+giac_rand(contextptr);
    // Now res is in [0,(RAND_MAX+1)^n-1]
    // Rescale in x1..x2
    return x1+_iquo(makevecteur(res*x2x1,pow(rand_max_plus_one,n)),contextptr);
  }
  gen rand_interval(const vecteur & v,bool entier,GIAC_CONTEXT){
    gen x1=v.front(),x2=v.back();
    if (x1==x2)
      return x1;
    if ((entier || xcas_mode(contextptr)==1) && is_integer(x1) && is_integer(x2) )
      return rand_integer_interval(x1,x2,contextptr);
#ifdef FXCG
    gen rand_max_plus_one=gen(rand_max2)+1;
#else
    static gen rand_max_plus_one=gen(rand_max2)+1;
#endif
#ifdef HAVE_LIBMPFR
    if (x1.type==_REAL && x2.type==_REAL){
      int n=mpfr_get_prec(x1._REALptr->inf);
      int nr=int(n*std::log(2.0)/std::log(rand_max2+1.0));
      gen xr=0;
      for (int i=0;i<=nr;++i){
	xr=xr*rand_max_plus_one+giac_rand(contextptr);
      }
      return x1+((x2-x1)*xr)/pow(rand_max_plus_one,nr+1);
    }
#endif
    gen x=evalf_double(x1,1,contextptr),y=evalf_double(x2,1,contextptr);
    if ( (x.type==_DOUBLE_) && (y.type==_DOUBLE_) ){
      double xd=x._DOUBLE_val,yd=y._DOUBLE_val;
      double xr= (giac_rand(contextptr)/evalf_double(rand_max_plus_one,1,contextptr)._DOUBLE_val)*(yd-xd)+xd;
      return xr;
    }
    return symb_rand(gen(v,_SEQ__VECT));
  }

  void shuffle(vector<int> & temp,GIAC_CONTEXT){
    int n=int(temp.size());
    // source wikipedia Fisher-Yates shuffle article
    for (int i=0;i<n-1;++i){
      // j ← random integer such that i ≤ j < n
      // exchange a[i] and a[j]
      int j=int(i+(giac_rand(contextptr)/double(rand_max2+1.0))*(n-i));
      std::swap(temp[i],temp[j]);
    }
  }

  vector<int> rand_k_n(int k,int n,bool sorted,GIAC_CONTEXT){
    if (k<=0 || n<=0 || k>n)
      return vector<int>(0);
    if (//n>=65536 && 
	k*double(k)<=n/4){
      vector<int> t(k),ts(k); 
      for (int essai=20;essai>=0;--essai){
	int i;
	for (i=0;i<k;++i)
	  ts[i]=t[i]=int(giac_rand(contextptr)/double(rand_max2+1.0)*n);
	sort(ts.begin(),ts.end());
	for (i=1;i<k;++i){
	  if (ts[i]==ts[i-1])
	    break;
	}
	if (i==k)
	  return sorted?ts:t;
      }
    }
    if (k>=n/3 || (sorted && k*std::log(double(k))>n) ){
      vector<int> t; t.reserve(k);
      // (algorithm suggested by O. Garet)
      while (n>0){
	int r=int(giac_rand(contextptr)/double(rand_max2+1.0)*n);
	if (r<n-k) // (n-k)/n=proba that the current n is not in the list
	  --n;
	else {
	  --n;
	  t.push_back(n);
	  --k;
	}
      }
      if (sorted)
	reverse(t.begin(),t.end());
      else
	shuffle(t);
      return t;
    }
    vector<bool> tab(n,true);
    vector<int> v(k);
    for (int j=0;j<k;++j){
      int r=-1;
      for (;;){
	r=int(giac_rand(contextptr)/double(rand_max2+1.0)*n);
	if (tab[r]){ tab[r]=false;  break; }
      }
      v[j]=r;
    }
    if (sorted)
      sort(v.begin(),v.end());
    return v;
  }
  
  vector<int> randperm(const int & n,GIAC_CONTEXT){
    //renvoie une permutation au hasard de long n
    vector<int> temp(n);
    for (int k=0;k<n;k++) 
      temp[k]=k;
    shuffle(temp,contextptr);
    return temp;
  }

  static gen rand_n_in_list(int n,const vecteur & v,GIAC_CONTEXT){
    n=absint(n);
    if (signed(v.size())<n)
      return gendimerr(contextptr);
#if 1
    vector<int> w=rand_k_n(n,int(v.size()),false,contextptr);
    vecteur res(n);
    for (int i=0;i<n;++i)
      res[i]=v[w[i]];
    return res;
#else
    // would be faster with randperm
    vecteur w(v);
    vecteur res;
    for (int i=0;i<n;++i){
      int tmp=int((double(giac_rand(contextptr))*w.size())/rand_max2);
      res.push_back(w[tmp]);
      w.erase(w.begin()+tmp);
    }
    return res;
#endif
  }
  gen _rand(const gen & args,GIAC_CONTEXT){
    int argsval;
    if (args.type==_INT_ && (argsval=args.val)){
#ifndef FXCG
      global * ptr; int c;
      if (argsval>0 && contextptr && (ptr=contextptr->globalptr) && ptr->_xcas_mode_!=3 && (c=ptr->_calc_mode_)!=-38 && c!=38) {
	tinymt32_t * rs=&ptr->_rand_seed;
	unsigned r;
	for (;;){
	  r=tinymt32_generate_uint32(rs) >> 1;
	  if (!(r>>31))
	    break;
	}
	return int(argsval*(r*inv_rand_max2_p1));
      }
#endif
      if (argsval<0)
	return -(xcas_mode(contextptr)==3)+int(argsval*(giac_rand(contextptr)/(rand_max2+1.0)));
      else
	return (xcas_mode(contextptr)==3 || abs_calc_mode(contextptr)==38)+int(argsval*(giac_rand(contextptr)/(rand_max2+1.0)));
    }
    if (args.type==_STRNG &&  args.subtype==-1) return  args;
    if (is_zero(args) && args.type!=_VECT)
      return giac_rand(contextptr);
    if (args.type==_ZINT)
      return rand_integer_interval(zero,args,contextptr);
    if (args.type==_FRAC)
      return _rand(args._FRACptr->num,contextptr)/args._FRACptr->den;
    if (args.type==_USER)
      return args._USERptr->rand(contextptr);
    if (args.is_symb_of_sommet(at_rootof))
      return vranm(1,args,contextptr)[0];
#ifndef USE_GMP_REPLACEMENTS
    if (args.is_symb_of_sommet(at_discreted))
      return vranm(1,args,contextptr)[0];
#endif
    if (args.type==_VECT && args._VECTptr->front()==at_multinomial) {
      vecteur v=*args._VECTptr;
      v.insert(v.begin(),1);
      return _randvector(v,contextptr)._VECTptr->at(0);
    }
    int nd=is_distribution(args);
    if (nd==1 && args.type==_FUNC)
      return randNorm(contextptr);
    if (args.is_symb_of_sommet(at_exp))
      return _randexp(args._SYMBptr->feuille,contextptr);
    if (nd && args.type==_SYMB){
      vecteur v=gen2vecteur(args._SYMBptr->feuille);
      if (nd==1){
	if (v.size()!=2)
	  return gensizeerr(contextptr);
	return randNorm(contextptr)*v[1]+v[0];
      }
      if (nd==2){
	if (v.size()!=2 || !is_integral(v[0]) || v[0].type!=_INT_ || (v[1]=evalf_double(v[1],1,contextptr)).type!=_DOUBLE_)
	  return gensizeerr(contextptr);
	return randbinomial(v[0].val,v[1]._DOUBLE_val,contextptr);
      }
      double p=giac_rand(contextptr)/(rand_max2+1.0);
      v.push_back(p);
      return icdf(nd)(gen(v,_SEQ__VECT),contextptr);
    }
    if (args.type==_VECT){ 
      if (args._VECTptr->empty()){
	if (xcas_mode(contextptr)==1)
	  return giac_rand(contextptr);
	else
	  return giac_rand(contextptr)/(rand_max2+1.0);
      }
      if (args.subtype==0){
	double d=giac_rand(contextptr)*double(args._VECTptr->size())/(rand_max2+1.0);
	return (*args._VECTptr)[int(d)];
      }
      vecteur & v=*args._VECTptr;
      int s=int(v.size());
      if ((nd=is_distribution(v[0]))){
	if (s==1)
	  return _rand(v[0],contextptr);
	return _rand(v[0] (gen(vecteur(v.begin()+1,v.end()),_SEQ__VECT),contextptr),contextptr);
      }
      if (s==2){ 
	if (v[0]==at_exp)
	  return _randexp(v[1],contextptr);
	if (v.front().type==_INT_ && v.back().type==_VECT){ // rand(n,list) choose n in list
	  return rand_n_in_list(v.front().val,*v.back()._VECTptr,contextptr);
	}
	if ( (v.back().type==_SYMB) && (v.back()._SYMBptr->sommet==at_interval) ){
	  // arg1=loi, arg2=intervalle
	}
	if (v[0].type==_DOUBLE_ && v[1].type==_INT_)
	  return _randvector(makesequence(v[1],symb_interval(0,1)),contextptr);
	return rand_interval(v,args.subtype==0,contextptr);
      }
      if (s==3 && v[0].type==_DOUBLE_ && v[1].type==_INT_ && v[2].type==_INT_)
	return _ranm(makesequence(v[1],v[2],symb_interval(0,1)),contextptr);
      if (s==3 && v[0].type==_INT_ && v[1].type==_INT_ && v[2].type==_INT_){ 
	// 3 integers expected, rand(n,min,max) choose n in min..max
	int n=v[0].val;
	int m=v[1].val;
	int M=v[2].val;
	if (m>M){ int tmp=m; m=M; M=tmp; }
#if 1
	vector<int> v=rand_k_n(n,M-m+1,false,contextptr);
	if (v.empty()) return gendimerr(contextptr);
	for (int i=0;i<n;++i)
	  v[i] += m;
	vecteur res;
	vector_int2vecteur(v,res);
	return res;
#else
	vecteur v;
	for (int i=m;i<=M;++i) v.push_back(i);
	return rand_n_in_list(n,v,contextptr);
#endif
      }
    }
    if ( (args.type==_SYMB) && args._SYMBptr->sommet==at_interval && args._SYMBptr->feuille.type==_VECT ){
      vecteur & v=*args._SYMBptr->feuille._VECTptr;
      return symb_program(vecteur(0),vecteur(0),symb_rand(gen(v,_SEQ__VECT)),contextptr);
      // return rand_interval(v);
    }
    return symb_rand(args);
  }
  static const char _rand_s []="rand";
  static define_unary_function_eval (__rand,&_rand,_rand_s);
  define_unary_function_ptr5( at_rand ,alias_at_rand,&__rand,0,true);

  static const char _random_s []="random";
  static define_unary_function_eval (__random,&_rand,_random_s);
  define_unary_function_ptr5( at_random ,alias_at_random,&__random,0,true);

  gen _randint(const gen & args,GIAC_CONTEXT){
    if (args.type==_INT_ || args.type==_ZINT)
      return (abs_calc_mode(contextptr)==38?0:1)+_rand(args,contextptr);
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (!is_integral(a) || !is_integral(b))
      return gentypeerr(contextptr);
    return (abs_calc_mode(contextptr)==38?a-1:a)+_rand(b-a+1,contextptr);
  }
  static const char _randint_s []="randint";
  static define_unary_function_eval (__randint,&_randint,_randint_s);
  define_unary_function_ptr5( at_randint ,alias_at_randint,&__randint,0,true);

  gen _randrange(const gen & args,GIAC_CONTEXT){
    if (args.type==_INT_)
      return (abs_calc_mode(contextptr)==38?-1:0)+_rand(args,contextptr);
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (!is_integral(a) || !is_integral(b))
      return gentypeerr(contextptr);
    return (abs_calc_mode(contextptr)==38?a-1:a)+_rand(b-a,contextptr);
  }
  static const char _randrange_s []="randrange";
  static define_unary_function_eval (__randrange,&_randrange,_randrange_s);
  define_unary_function_ptr5( at_randrange ,alias_at_randrange,&__randrange,0,true);

  gen _choice(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT || args.subtype==_SEQ__VECT || args._VECTptr->empty())
      return gensizeerr(contextptr);
    int n=int(args._VECTptr->size());
    gen g=_rand(n,contextptr)+(abs_calc_mode(contextptr)==38?-1:0);
    if (g.type!=_INT_ || g.val<0 || g.val>=n)
      return gendimerr(contextptr);
    return args[g.val];
  }
  static const char _choice_s []="choice";
  static define_unary_function_eval (__choice,&_choice,_choice_s);
  define_unary_function_ptr5( at_choice ,alias_at_choice,&__choice,0,true);

  gen _shuffle(const gen & a,GIAC_CONTEXT){
    gen args(a);
    if (is_integral(args))
      return _randperm(args,contextptr);
    if (args.type!=_VECT || args._VECTptr->empty())
      return gensizeerr(contextptr);
    vecteur v(*args._VECTptr);
    int n=int(v.size());
    vecteur w(n);
    vector<int> p=randperm(n,contextptr);
    for (int i=0;i<n;++i){
      w[i]=v[p[i]];
    }
    return gen(w,args.subtype);
  }
  static const char _shuffle_s []="shuffle";
  static define_unary_function_eval (__shuffle,&_shuffle,_shuffle_s);
  define_unary_function_ptr5( at_shuffle ,alias_at_shuffle,&__shuffle,0,true);

#ifndef USE_GMP_REPLACEMENTS
  gen _sample(const gen & args,GIAC_CONTEXT){
    if (args.is_symb_of_sommet(at_discreted) || is_distribution(args)>0)
      return _rand(args,contextptr);
    if (args.type==_SYMB || args.type==_IDNT)
      return _randvector(makesequence(1,args),contextptr)._VECTptr->front();
    if (args.type!=_VECT || args._VECTptr->size()<2)
      return gensizeerr(contextptr);
    vecteur &argv=*args._VECTptr;
    gen a=argv.front(),b=argv.back();
    if (a==at_multinomial) {
      if (argv.size()==3) {
        vecteur v=argv;
        v.insert(v.begin(),1);
        return _randvector(v,contextptr)._VECTptr->at(0);
      } if (argv.size()==4 && b.is_integer()) {
        return _randvector(makesequence(b,at_multinomial,argv[1],argv[2]),contextptr);
      }
      return gensizeerr(contextptr);
    }
    if (args._VECTptr->size()!=2 || !is_integral(b) || b.type==_ZINT || b.val<0)
      return gensizeerr(contextptr);
    if (a.is_symb_of_sommet(at_discreted) || is_distribution(a)>0 || a.type==_SYMB || a.type==_IDNT)
      return _randvector(makesequence(b,a),contextptr);
    if (a.type!=_VECT)
      return gensizeerr(contextptr);
    return _rand(makesequence(b,a),contextptr);
  }
  static const char _sample_s []="sample";
  static define_unary_function_eval (__sample,&_sample,_sample_s);
  define_unary_function_ptr5( at_sample ,alias_at_sample,&__sample,0,true);
#endif

  gen _srand(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_INT_){
      int n=args.val;
#ifdef GIAC_HAS_STO_38
      n = (1000000000*ulonglong(n))% 2147483647;
#endif
#ifndef FXCG
      srand(n);
#endif
      rand_seed(n,contextptr);
      return args;
    }
    else {
#if defined RTOS_THREADX || defined BESTA_OS
      int t=PrimeGetNow();
#else
#ifdef FXCG
      int t=RTC_GetTicks();
#else
      int t=int(time(0));
#endif // FXCG
#endif // RTOS/BESTA
      t = (1000000000*ulonglong(t))% 2147483647;
#ifdef VISUALC
      // srand48(t);
#endif
      rand_seed(t,contextptr);
#ifndef FXCG
      srand(t);
#endif
      return t;
    }
  }
  static const char _srand_s []="srand";
  static define_unary_function_eval (__srand,&_srand,_srand_s);
  define_unary_function_ptr5( at_srand ,alias_at_srand ,&__srand,0,T_RETURN);

  static gen symb_char(const gen & args){
    return symbolic(at_char,args);
  }
  gen _char(const gen & args_,GIAC_CONTEXT){
    if ( args_.type==_STRNG &&  args_.subtype==-1) return  args_;
    string s;
    gen args(args_);
    if (is_integral(args)){
      s += char(args.val) ;
    }
    else {
      if (args.type==_VECT){
	vecteur v=*args._VECTptr;
	iterateur it=v.begin(),itend=v.end();
	for (;it!=itend;++it){
	  if (is_integral(*it))
	    s += char(it->val);
	  else return gensizeerr(contextptr);
	}
      }
      else return gensizeerr(contextptr);
    }
    gen tmp=string2gen(s,false);
    return tmp;
  }
  static const char _char_s []="char";
  static define_unary_function_eval (__char,&_char,_char_s);
  define_unary_function_ptr5( at_char ,alias_at_char,&__char,0,true);

  static const char _chr_s []="chr";
  static define_unary_function_eval (__chr,&_char,_chr_s);
  define_unary_function_ptr5( at_chr ,alias_at_chr,&__chr,0,true);

  static gen symb_asc(const gen & args){
    return symbolic(at_asc,args);
  }
  gen _asc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_STRNG){
      int l=int(args._STRNGptr->size());
      vecteur v(l);
      for (int i=0;i<l;++i)
	v[i]=int( (unsigned char) ((*args._STRNGptr)[i]));
      if (abs_calc_mode(contextptr)==38)
	return gen(v,_LIST__VECT);
      return v;
    }
    if (args.type==_VECT){
      if ( (args._VECTptr->size()!=2) ||(args._VECTptr->front().type!=_STRNG) || (args._VECTptr->back().type!=_INT_) )
	return gensizeerr(gettext("asc"));
      return int( (unsigned char) (*args._VECTptr->front()._STRNGptr)[args._VECTptr->back().val]);
    }
    else return symb_asc(args);
  }

  static const char _asc_s []="asc";
  static define_unary_function_eval (__asc,&_asc,_asc_s);
  define_unary_function_ptr5( at_asc ,alias_at_asc,&__asc,0,true);

  static gen symb_map(const gen & args){
    return symbolic(at_map,args);
  }
  gen _map(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_map(args);
    vecteur v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gentoofewargs("");
    gen objet=v.front();
    gen to_map=v[1];
    // FIXME: should have maple_map and mupad_map functions
    if (xcas_mode(contextptr)==1 || (to_map.type!=_FUNC && !to_map.is_symb_of_sommet(at_program) && (objet.type==_FUNC || objet.is_symb_of_sommet(at_program)))){
      objet=v[1];
      to_map=v.front();
    }
    bool matrix = ckmatrix(objet) && s>2;
    if (matrix){
      matrix=false;
      for (int i=2;i<s;++i){
	if (v[i]==at_matrix){
	  v.erase(v.begin()+i);
	  --s;
	  matrix=true;
	  break;
	}
      }
    }
    if (to_map.type==_VECT)
      return gensizeerr(contextptr);
    if (v.size()==2){
      if (objet.type==_SYMB){
	gen & f=objet._SYMBptr->feuille;
	gen tmp;
	if (xcas_mode(contextptr)==1)
	  tmp=_map(makevecteur(to_map,f),contextptr);
	else
	  tmp=_map(makevecteur(f,to_map),contextptr);
	if (f.type==_VECT && tmp.type==_VECT)
	  tmp.subtype=f.subtype;
	if (is_equal(objet) || objet._SYMBptr->sommet==at_same)
	  return symbolic(at_equal,tmp);
	return objet._SYMBptr->sommet(tmp,contextptr);
      }
      // if (to_map.type==_FUNC) return apply(objet,*to_map._FUNCptr);
      if (objet.type==_POLY){
	int dim=objet._POLYptr->dim;
	polynome res(dim);
	vector< monomial<gen> >::const_iterator it=objet._POLYptr->coord.begin(),itend=objet._POLYptr->coord.end();
	res.coord.reserve(itend-it);
	vecteur argv(dim+1);
	for (;it!=itend;++it){
	  argv[0]=it->value;
	  index_t::const_iterator i=it->index.begin();
	  for (int j=0;j<dim;++j,++i)
	    argv[j+1]=*i;
	  gen g=to_map(gen(argv,_SEQ__VECT),contextptr);
	  if (!is_zero(g))
	    res.coord.push_back(monomial<gen>(g,it->index));
	}
	return res;
      }
      if (objet.type!=_VECT)
	return to_map(objet,contextptr);
      const_iterateur it=objet._VECTptr->begin(),itend=objet._VECTptr->end();
      vecteur res;
      res.reserve(itend-it);
      for (;it!=itend;++it){
	if (matrix && it->type==_VECT){
	  const vecteur & tmp = *it->_VECTptr;
	  const_iterateur jt=tmp.begin(),jtend=tmp.end();
	  vecteur tmpres;
	  tmpres.reserve(jtend-jt);
	  for (;jt!=jtend;++jt){
	    tmpres.push_back(to_map(*jt,contextptr));
	  }
	  res.push_back(tmpres);
	}
	else
	  res.push_back(to_map(*it,contextptr));
      }
      return res;
    }
    if (objet.type==_POLY){
      int dim=objet._POLYptr->dim;
      vecteur opt(v.begin()+2,v.end());
      opt=mergevecteur(vecteur(dim+1),opt);
      polynome res(dim);
      vector< monomial<gen> >::const_iterator it=objet._POLYptr->coord.begin(),itend=objet._POLYptr->coord.end();
      res.coord.reserve(itend-it);
      for (;it!=itend;++it){
	opt[0]=it->value;
	index_t::const_iterator i=it->index.begin();
	for (int j=0;j<dim;++j,++i)
	  opt[j+1]=*i;
	gen g=to_map(gen(opt,_SEQ__VECT),contextptr);
	if (!is_zero(g))
	  res.coord.push_back(monomial<gen>(g,it->index));
      }
      return res;
    }
    vecteur opt(v.begin()+1,v.end());
    opt[0]=objet;
    if (objet.type!=_VECT)
      return to_map(opt,contextptr);
    bool multimap=!matrix && opt.size()>1 && ckmatrix(opt);
    const_iterateur it=objet._VECTptr->begin(),itend=objet._VECTptr->end();
    vecteur res;
    res.reserve(itend-it);
    for (int k=0;it!=itend;++it,++k){
      if (matrix && it->type==_VECT){
	const vecteur & tmp = *it->_VECTptr;
	const_iterateur jt=tmp.begin(),jtend=tmp.end();
	vecteur tmpres;
	tmpres.reserve(jtend-jt);
	for (;jt!=jtend;++jt){
	  opt[0]=*jt;
	  tmpres.push_back(to_map(gen(opt,_SEQ__VECT),contextptr));
	}
	res.push_back(tmpres);
      }
      else {
	opt[0]=*it;
	gen arg=gen(opt,_SEQ__VECT);
	if (multimap){
	  vecteur & v=*arg._VECTptr;
	  for (int j=1;j<v.size();++j)
	    v[j]=(*v[j]._VECTptr)[k];
	} 
	res.push_back(to_map(arg,contextptr));
      }
    }
    return res;
  }
  static const char _map_s []="map";
  static define_unary_function_eval (__map,&_map,_map_s);
  define_unary_function_ptr5( at_map ,alias_at_map,&__map,0,true);
  
  static gen symb_apply(const gen & args){
    return symbolic(at_apply,args);
  }
  gen _apply(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_apply(args);
    if (args._VECTptr->empty())
      return gensizeerr(gettext("apply"));
    vecteur v=*args._VECTptr;
    gen to_apply=v.front();
    int n=to_apply.subtype;
    int n2=int(v.size());
    int subt=0;
    if (n2>=2 && v[1].type==_VECT)
      subt=v[1].subtype;
    for (int i=2;i<n2;++i){
      if (v[i]==at_matrix){
	swapgen(v[0],v[1]);
	return _map(gen(v,args.subtype),contextptr);
      }
    }
    if (to_apply.type!=_FUNC)
      n=n2-1;
    int nargs=1;
    if (to_apply.is_symb_of_sommet(at_program) && to_apply._SYMBptr->feuille[0].type==_VECT)
      nargs=to_apply._SYMBptr->feuille[0]._VECTptr->size();
    if (n && (n2==n+1) ){
      vecteur res;
      for (int i=0;;++i){
	vecteur tmp;
	bool finished=true;
	for (int j=1;j<=n;++j){
	  gen g=v[j];
	  if (g.type==_STRNG){
	    vecteur w(g._STRNGptr->size());
	    for (size_t i=0;i<g._STRNGptr->size();++i)
	      w[i]=string2gen(g._STRNGptr->substr(i,1),false);
	    g=w;
	  }
	  if (g.type!=_VECT)
	    tmp.push_back(g);
	  else {
	    if (signed(g._VECTptr->size())>i){
	      finished=false;
	      tmp.push_back((*g._VECTptr)[i]);
	    }
	    else
	      tmp.push_back(zero);
	  }
	}
	if (finished)
	  break;
	if (n==1){
	  gen tmp1=tmp.front();
	  if (nargs>1 && tmp1.type==_VECT) // for apply((j,k)->j*k,matrix 2 cols)
	    tmp1.subtype=_SEQ__VECT;
	  res.push_back(to_apply(tmp1,contextptr));
	}
	else
	  res.push_back(to_apply(tmp,contextptr));
      }
      return gen(res,subt);
    }
    else
      return gensizeerr(contextptr);
    return 0;
  }
  static const char _apply_s []="apply";
  static define_unary_function_eval (__apply,&_apply,_apply_s);
  define_unary_function_ptr5( at_apply ,alias_at_apply,&__apply,0,true);
  
  // static gen symb_makelist(const gen & args){  return symbolic(at_makelist,args);  }
  gen _makelist(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_INT_){
      if (args.val>=0 && args.val<LIST_SIZE_LIMIT)
	return vecteur(args.val);
      if (args.val<0 && args.val>-LIST_SIZE_LIMIT){
	gen res=new_ref_vecteur(vecteur(0));
	res._VECTptr->reserve(-args.val);
	return res;
      }
    }
    if (args.type!=_VECT)
      return gensizeerr();
    vecteur v(*args._VECTptr);
    int s=int(v.size());
    if (s<2)
      return gensizeerr(contextptr);
    gen f(v[0]),debut,fin,step(1);
    if (v[1].is_symb_of_sommet(at_interval)){
      debut=v[1]._SYMBptr->feuille._VECTptr->front();
      fin=v[1]._SYMBptr->feuille._VECTptr->back();
      if (s>2)
	step=v[2];
    }
    else {
      if (s<3)
	return gensizeerr(contextptr);
      debut=v[1];
      fin=v[2];
      if (s>3)
	step=v[3];
    }
    if (is_zero(step))
      return gensizeerr(gettext("Invalid null step"));
    if (is_greater((fin-debut)/step,LIST_SIZE_LIMIT,contextptr))
      return gendimerr(contextptr);
    vecteur w;
    if (is_greater(fin,debut,contextptr)){
      step=abs(step,contextptr);
      for (gen i=debut;is_greater(fin,i,contextptr);i=i+step)
	w.push_back(f(i,contextptr));
    }
    else {
      step=-abs(step,contextptr);
      for (gen i=debut;is_greater(i,fin,contextptr);i=i+step)
	w.push_back(f(i,contextptr));
    }
    return w;
  }
  static const char _makelist_s []="makelist";
  static define_unary_function_eval (__makelist,&_makelist,_makelist_s);
  define_unary_function_ptr5( at_makelist ,alias_at_makelist,&__makelist,0,true);

  static gen symb_interval(const gen & args){
    return symbolic(at_interval,args);
  }
  gen symb_interval(const gen & a,const gen & b){
    return symbolic(at_interval,gen(makevecteur(a,b),_SEQ__VECT));
  }
  gen _interval(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symb_interval(args);
  }
  static const char _interval_s []=" .. ";
  static define_unary_function_eval4_index (56,__interval,&_interval,_interval_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_interval ,alias_at_interval ,&__interval);

  gen _leftopen_interval(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen g=symb_interval(args);
    interval2realset(g,true,false);
    return g;
  }
  static const char _leftopen_interval_s []=" ..! ";
  static define_unary_function_eval4 (__leftopen_interval,&_leftopen_interval,_leftopen_interval_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_leftopen_interval ,alias_at_leftopen_interval ,&__leftopen_interval);
  
  gen _rightopen_interval(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen g=symb_interval(args);
    interval2realset(g,false,true);
    return g;
  }
  static const char _rightopen_interval_s []=" !!. ";
  static define_unary_function_eval4 (__rightopen_interval,&_rightopen_interval,_rightopen_interval_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_rightopen_interval ,alias_at_rightopen_interval ,&__rightopen_interval);
  
  gen _leftrightopen_interval(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen g=symb_interval(args);
    interval2realset(g,true,true);
    return g;
  }
  static const char _leftrightopen_interval_s []=" !.! ";
  static define_unary_function_eval4 (__leftrightopen_interval,&_leftrightopen_interval,_leftrightopen_interval_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_leftrightopen_interval ,alias_at_leftrightopen_interval ,&__leftrightopen_interval);
  
  static string printascomment(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_STRNG)
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    string chaine=*feuille._STRNGptr;
    int s=int(chaine.size());
    if ( (xcas_mode(contextptr)==1) || (xcas_mode(contextptr)==1+_DECALAGE)){
      string res("# ");
      for (int i=0;i<s;++i){
	if ((i==s-1)||(chaine[i]!='\n'))
	  res +=chaine[i];
	else
	  res += indent(contextptr)+"# ";
      }
      return res;
    }
    int l=int(chaine.find_first_of('\n'));
    if ((l<0)|| (l>=s))
        return "//"+chaine + indent(contextptr);
    return "/*"+chaine+"*/";
  }
  static gen symb_comment(const gen & args){
    return symbolic(at_comment,args);
  }
  gen _comment(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symb_comment(args);
  }
  static const char _comment_s []="comment";
  static define_unary_function_eval2 (__comment,&_comment,_comment_s,&printascomment);
  define_unary_function_ptr5( at_comment ,alias_at_comment ,&__comment,0,true);

  // static gen symb_throw(const gen & args){  return symbolic(at_throw,args);  }
  gen _throw(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return gensizeerr(args.print(contextptr));
  }
  static const char _throw_s []="throw";
  static define_unary_function_eval (__throw,&_throw,_throw_s);
  define_unary_function_ptr5( at_throw ,alias_at_throw ,&__throw,0,T_RETURN);

  static string texprintasunion(const gen & g,const char * s,GIAC_CONTEXT){
    return texprintsommetasoperator(g,"\\cup",contextptr);
  }
  gen symb_union(const gen & args){
    return symbolic(at_union,args);
  }
  gen _union(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur & v =*args._VECTptr;
    if (v.empty())
      return args;
    if (v.size()==1 && v.front().type==_VECT)
      return gen(v,_SET__VECT).eval(1,contextptr);
    if (v.size()==1)
      return v[0];
    if (v.size()!=2)
      return symb_union(args);
    gen a=v.front(),b=v.back();
    interval2realset(a); interval2realset(b);
    if (a==b && a.subtype!=_REALSET__VECT){
      if (a.type==_VECT){
        if (a.subtype!=_SET__VECT && a.subtype!=_REALSET__VECT)
          chk_set(a);
      }
      return a;
    }
#if defined HAVE_LIBMPFI && !defined NO_RTTI      
    if (a.type==_REAL && b.type==_REAL){
      if (real_interval * aptr=dynamic_cast<real_interval *>(a._REALptr)){
	if (real_interval * bptr=dynamic_cast<real_interval *>(b._REALptr)){
	  mpfi_t tmp;
	  mpfi_init2(tmp,giacmin(mpfi_get_prec(aptr->infsup),mpfi_get_prec(bptr->infsup)));
	  mpfi_union(tmp,aptr->infsup,bptr->infsup);
	  gen res=real_interval(tmp);
	  mpfi_clear(tmp);
	  return res;
	}
	else {
	  if (contains(a,b))
	    return a;
	  else
	    return _union(makesequence(a,eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr)),contextptr);
	}
      }
      if (contains(b,a))
	return b;
      else
	return _union(makesequence(a,eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr)),contextptr);
    }
    if (a.type==_REAL){
      if (contains(a,b))
	return a;
      else
	return _union(makesequence(a,eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr)),contextptr);
    }
    if (b.type==_REAL){
      if (contains(b,a))
	return b;
      else
	return _union(makesequence(a,eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr)),contextptr);
    }
#endif
    if (a.type!=_VECT || b.type!=_VECT){
      if (a.is_symb_of_sommet(at_union)){
        vecteur af=gen2vecteur(a._SYMBptr->feuille);
        if (b.is_symb_of_sommet(at_union))
          af=mergevecteur(af,gen2vecteur(b._SYMBptr->feuille));
        else
          af.push_back(b);
        chk_set(af);
        return symbolic(at_union,gen(af,_SET__VECT));
      }
      if (b.is_symb_of_sommet(at_union)){
        vecteur bf=gen2vecteur(b._SYMBptr->feuille);
        bf.insert(bf.begin(),a);
        chk_set(bf);
        return symbolic(at_union,gen(bf,_SET__VECT));
      }
      return symb_union(args); //gensizeerr(gettext("Union"));
    } else {
      if (a.subtype==_REALSET__VECT || b.subtype==_REALSET__VECT){
        if (!convert_realset(a,contextptr) || !convert_realset(b,contextptr))
          return gensizeerr(contextptr);
        vecteur w;
        if (!realset_glue(*a._VECTptr,*b._VECTptr,w,contextptr))
          return gensizeerr(contextptr);
        return vecteur2realset(w,contextptr); 
      } 
      chk_set(a);
      chk_set(b);
      vecteur v;
      const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
      const_iterateur jt=b._VECTptr->begin(),jtend=b._VECTptr->end();
      for (;it!=itend && jt!=jtend;){
        if (*it==*jt){
          v.push_back(*it);
          ++it; ++jt;
          continue;
        }
        if (set_sort(*it,*jt)){
          v.push_back(*it);
          ++it;
        }
        else {
          v.push_back(*jt);
          ++jt;
        }
      }
      for (;it!=itend;++it)
        v.push_back(*it);
      for (;jt!=jtend;++jt)
        v.push_back(*jt);
      return gen(v,_SET__VECT);
    }
  }
  static const char _union_s []=" union ";
  static define_unary_function_eval4_index (58,__union,&_union,_union_s,&printsommetasoperator,&texprintasunion);
  define_unary_function_ptr( at_union ,alias_at_union ,&__union);

  void chk_set(vecteur & av){
    if (av.empty()) return;
    sort(av.begin(),av.end(),set_sort);
    vecteur res; res.reserve(av.size());
    gen prec=av[0];
    res.push_back(prec);
    for (int i=1;i<av.size();++i){
      if (prec.type==_VECT && prec.subtype==_LINE__VECT && av[i].type==_VECT && av[i].subtype==_LINE__VECT){
        if (*prec._VECTptr==*av[i]._VECTptr)
          continue;
      }
      else if (av[i]==prec)
        continue;
      prec=av[i];
      res.push_back(prec);
    }
    av.swap(res);
  }    

  void chk_set(gen & a){
    if (a.type==_VECT && a.subtype!=_SET__VECT && !a._VECTptr->empty()){
      vecteur av=*a._VECTptr;
      //comprim(av);a=av;
      chk_set(av);
      a=gen(av,_SET__VECT);
    }
  }
  
  bool maybe_set(const gen & g){
    if (g.type==_VECT && g.subtype==_SET__VECT)
      return true;
    if (g.type==_VECT){
      const vecteur & v=*g._VECTptr;
      for (int i=0;i<v.size();++i){
        if (!maybe_set(v[i]))
          return false;
      }
      return true;
    }
    if (g.type==_IDNT)
      return true;
    if (g.type!=_SYMB)
      return false;
    const unary_function_ptr & u=g._SYMBptr->sommet;
    if (u!=at_intersect && u!=at_union && u!=at_complement && u!=at_symmetric_difference && u!=at_minus)
      return false;
    return maybe_set(g._SYMBptr->feuille);
  }

  // search interval in a or value a in the list of intervals in b
  bool realset_in(const gen & a,const vecteur &b,GIAC_CONTEXT){
    gen m(a),M(a);
    if (a.type==_VECT){
      if (a._VECTptr->size()!=2)
        return false;
      m=a._VECTptr->front(),M=a._VECTptr->back();
    }
    for (int i=0;i<b.size();++i){
      gen cur=b[i];
      if (cur._VECTptr->size()!=2)
        return false;
      gen x=cur._VECTptr->front(),y=cur._VECTptr->back();
      if (is_greater(m,x,contextptr) && is_greater(y,M,contextptr))
        return true;
    }
    return false;
  }

  // is realset in u contained in realset in v?
  bool realset_in(const vecteur &u,const vecteur &v,GIAC_CONTEXT){
    if (u.size()<2 || v.size()<2)
      return false;
    gen ai=u[u.size()-2],bi=v[v.size()-2],aex=u.back(),bex=v.back();
    if (ai.type!=_VECT || bi.type!=_VECT || aex.type!=_VECT || bex.type!=_VECT)
      return false;
    vecteur aint=*ai._VECTptr;
    vecteur bint=*bi._VECTptr;
    // find an interval of b containing each interval of a
    for (int i=0;i<aint.size();++i){
      if (!realset_in(aint[i],bint,contextptr))
        return false;
    }
    // check that excluded values of b are not in a
    for (int i=0;i<bex._VECTptr->size();++i){
      gen cur=bex[i];
      if (realset_in(cur,aint,contextptr)){
        if (!binary_search(aex._VECTptr->begin(),aex._VECTptr->end(),cur,set_sort))
          return false;
      }
    }
    return true;
  }

  // 0 equal, 1 a contains b, 2 b contains a, negative: non comparable, -2 explicit sets a_ not included in b_ and b_ not included in a_, -3 too many idnts
  int set_compare(const gen & a_,const gen &b_,GIAC_CONTEXT){
    if (a_==b_)
      return 0;
    if (a_.type==_VECT && b_.type==_VECT){
      if (a_.subtype==_REALSET__VECT || b_.subtype==_REALSET__VECT){
        gen a(a_),b(b_);
        if (!convert_realset(a,contextptr) || !convert_realset(b,contextptr))
          return -4;
        bool ainb=realset_in(*a._VECTptr,*b._VECTptr,contextptr),bina=realset_in(*b._VECTptr,*a._VECTptr,contextptr);
        if (ainb && bina)
          return 0;
        if (ainb)
          return 2;
        if (bina)
          return 1;
        return -2;
      } 
      vecteur a(*a_._VECTptr); chk_set(a);
      vecteur b(*b_._VECTptr); chk_set(b);
      if (a==b)
        return 0;
      int as=a.size(),bs=b.size();
      const_iterateur it,itend,jt,jtend;
      if (as>bs){
        it=a.begin();itend=a.end();jt=b.begin();jtend=b.end();
      } else {
        it=b.begin();itend=b.end();jt=a.begin();jtend=a.end();
      }
      for (;it!=itend && jt!=jtend;){
        if (*it==*jt){
          ++it; ++jt;
          continue;
        }
        if (set_sort(*jt,*it))
          return -2;
        ++it;
      }
      return as>bs?1:2;
    }
    if ( (a_.type==_IDNT || a_.type==_SYMB) && maybe_set(a_) && (b_.type==_IDNT || b_.type==_SYMB) && maybe_set(b_) ){
      vecteur l(lidnt(a_));
      lidnt(b_,l,false);
      int ls=l.size();
      if (ls>SET_COMPARE_MAXIDNT)
        return -3;
      unsigned ntries=1<<ls;
      int comp=0;
      gen a(set2logic(a_,contextptr)),b(set2logic(b_,contextptr));
      vecteur vals(ls);
      for (unsigned i=0;i<ntries;++i){
        unsigned I(i);
        for (int j=0;j<ls;++j){
          vals[j]=int(I%2);
          I/=2;
        }
        gen aa=subst(a,l,vals,false,contextptr);
        aa=eval(aa,1,contextptr);
        if (aa.type!=_INT_)
          return -1;
        gen bb=subst(b,l,vals,false,contextptr);
        bb=eval(bb,1,contextptr);
        if (bb.type!=_INT_)
          return -1;
        if (aa.val==bb.val)
          continue;
        if (aa.val==1){
          if (comp==2)
            return -1;
          comp=1;
          continue;
        }
        if (bb.val==1){
          if (comp==1)
            return -1;
          comp=2;
          continue;
        }
      }
      return comp;
    }
    return -1;
  }
  gen set_simplify(const gen & g,GIAC_CONTEXT){
    if (!maybe_set(g))
      return g;
    vecteur l(lidnt(g));
    int ls=l.size();
    if (ls>SET_COMPARE_MAXIDNT)
      return g;
    unsigned ntries=1<<ls;
    int comp=0;
    gen a(set2logic(g,contextptr));
    vecteur vals(ls);
    vector<bool> res(ntries);
    for (unsigned i=0;i<ntries;++i){
      unsigned I(i);
      for (int j=0;j<ls;++j){
        vals[j]=int(I%2);
        I/=2;
      }
      gen aa=subst(a,l,vals,false,contextptr);
      aa=eval(aa,1,contextptr);
      if (aa.type!=_INT_)
        return g;
      res[i]=aa.val;
    }
    // check if we can eliminate variables
    vector<bool> vars_present(ls,true);
    for (int var=0;var<ls;++var){
      int V=1<<var,i;
      // will make each test 2 times, but this is simpler code
      for (i=0;i<ntries;++i){
        int i1=i|V; // set bit at position var
        int i2=i&(~V); // clear bit at position var
        if (res[i1]!=res[i2]) // compare
          break;
      }
      if (i==ntries){
        // does not depend on var
        vars_present[var]=false;
      }
    }
    // generate global union
    vecteur result; 
    vecteur inter;
    bool emptyset=true;
    for (int i=0;i<ntries;++i){
      if (!res[i])
        continue;
      emptyset=false;
      unsigned I(i);
      inter.clear();
      for (int j=0;j<ls;++j,I/=2){
        if (!vars_present[j])
          continue;
        if (I%2)
          inter.push_back(l[j]);
        else
          inter.push_back(symbolic(at_complement,l[j]));
      }
      if (inter.empty())
        continue;
      if (inter.size()==1)
        result.push_back(inter[0]);
      else
        result.push_back(symbolic(at_intersect,gen(inter,_SEQ__VECT)));
    }
    chk_set(result); // sort and eliminate duplicate
    if (result.empty()){
      gen g(result,_SET__VECT);
      if (!emptyset)
        g=symbolic(at_complement,g);
      return g;
    }
    return symbolic(at_union,result);
  }
  gen _set_compare(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ((args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gensizeerr();
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    return set_compare(a,b,contextptr);
  }
  static const char _set_compare_s []="set_compare";
  static define_unary_function_eval (__set_compare,&_set_compare,_set_compare_s);
  define_unary_function_ptr5( at_set_compare ,alias_at_set_compare ,&__set_compare,0,true);
  
  static string texprintasintersect(const gen & g,const char * s,GIAC_CONTEXT){
    return texprintsommetasoperator(g,"\\cap",contextptr);
  }
  gen symb_intersect(const gen & args){
    return symbolic(at_intersect,args);
  }
  gen _intersect(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ((args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gensizeerr();
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    interval2realset(a); interval2realset(b);
    if (a==b && a.subtype!=_REALSET__VECT){
      if (a.type==_VECT){
        if (a.subtype!=_SET__VECT && a.subtype!=_REALSET__VECT)
          chk_set(a);
      }
      return a;
    }
#if defined HAVE_LIBMPFI && !defined NO_RTTI      
    if (a.type==_REAL && b.type==_REAL){
      if (real_interval * aptr=dynamic_cast<real_interval *>(a._REALptr)){
	if (real_interval * bptr=dynamic_cast<real_interval *>(b._REALptr)){
	  mpfi_t tmp;
	  mpfi_init2(tmp,giacmin(mpfi_get_prec(aptr->infsup),mpfi_get_prec(bptr->infsup)));
	  mpfi_intersect(tmp,aptr->infsup,bptr->infsup);
	  gen res;
	  if (mpfi_is_empty(tmp))
	    res=vecteur(0);
	  else
	    res=real_interval(tmp);
	  mpfi_clear(tmp);
	  return res;
	}
	else {
	  if (contains(a,b))
	    return eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr);
	  return vecteur(0);
	}
      }
      if (contains(b,a))
	return eval(gen(makevecteur(a,a),_INTERVAL__VECT),1,contextptr);
      return vecteur(0);
    }
    if (a.type==_REAL){
      if (contains(a,b))
	return eval(gen(makevecteur(b,b),_INTERVAL__VECT),1,contextptr);
      else
	return vecteur(0);
    }
    if (b.type==_REAL){
      if (contains(b,a))
	return a;
      else
	return vecteur(0);
    }
#endif
    if ( a.type==_VECT && b.type==_VECT){
      if (a.subtype==_REALSET__VECT || b.subtype==_REALSET__VECT){
        if (!convert_realset(a,contextptr) || !convert_realset(b,contextptr))
          return gensizeerr(contextptr);
        vecteur w;
        realset_inter(*a._VECTptr,*b._VECTptr,w,contextptr);
        return vecteur2realset(w,contextptr);
      }
      chk_set(a);
      chk_set(b);
      vecteur v;
      const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
      const_iterateur jt=b._VECTptr->begin(),jtend=b._VECTptr->end();
      for (;it!=itend && jt!=jtend;){
        if (*it==*jt){
          v.push_back(*it);
          ++it; ++jt;
          continue;
        }
        if (set_sort(*it,*jt))
          ++it;
        else
          ++jt;
      }
      return gen(v,_SET__VECT);
    }
    if (a.is_symb_of_sommet(at_intersect)){
      vecteur af=gen2vecteur(a._SYMBptr->feuille);
      if (b.is_symb_of_sommet(at_intersect))
        af=mergevecteur(af,gen2vecteur(b._SYMBptr->feuille));
      else
        af.push_back(b);
      chk_set(af);
      return symbolic(at_intersect,gen(af,_SET__VECT));
    }
    if (b.is_symb_of_sommet(at_intersect)){
      vecteur bf=gen2vecteur(b._SYMBptr->feuille);
      bf.insert(bf.begin(),a);
      chk_set(bf);
      return symbolic(at_intersect,gen(bf,_SET__VECT));
    }
    return symb_intersect(args); // gensizeerr(contextptr);
  }
  static const char _intersect_s []=" intersect ";
  static define_unary_function_eval4_index (62,__intersect,&_intersect,_intersect_s,&printsommetasoperator,&texprintasintersect);
  define_unary_function_ptr( at_intersect ,alias_at_intersect ,&__intersect);

  bool realset_complement(const vecteur & u,vecteur & w,GIAC_CONTEXT){
    if (u.size()<2)
      return false;
    gen u1=u[u.size()-2],u2=u.back();
    if (u1.type!=_VECT || u2.type!=_VECT)
      return false;
    vecteur uint(*u1._VECTptr),uex(*u2._VECTptr);
    vecteur wint,wex;
    gen g(minus_inf);
    for (int i=0;i<uint.size();++i){
      gen cur=uint[i];
      gen m=cur[0],M=cur[1];
      if (i!=0 || m!=minus_inf)
        wint.push_back(gen(makevecteur(g,m),_LINE__VECT));
      g=M;
      if (m!=minus_inf && !binary_search(uex.begin(),uex.end(),m,set_sort))
        wex.push_back(m);
      if (M!=plus_inf && !binary_search(uex.begin(),uex.end(),M,set_sort))
        wex.push_back(M);
    }
    if (g!=plus_inf)
      wint.push_back(gen(makevecteur(g,plus_inf),_LINE__VECT));
    w=makevecteur(wint,wex);
    return true;
  }
  static string texprintascomplement(const gen & g,const char * s,GIAC_CONTEXT){
    return "\\stcomp{"+gen2tex(g,contextptr)+"}";
  }
  gen _complement(const gen & args_,GIAC_CONTEXT){
    gen args(args_);
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.is_symb_of_sommet(at_complement)){
      gen f=args._SYMBptr->feuille;
      if (f.type==_VECT && f.subtype==_SEQ__VECT && f._VECTptr->size()==2)
        return f[0];
      return f;
    }
    interval2realset(args);
    if (args.type==_VECT && args.subtype==_REALSET__VECT){
      vecteur w;
      realset_complement(*args._VECTptr,w,contextptr);
      return vecteur2realset(w,contextptr);
    }
    if (args.type==_VECT && args._VECTptr->size()==2 && args.subtype==_SEQ__VECT){
      if (is_exactly_zero(_is_included(args,contextptr)))
        return gensizeerr(gettext("not included!"));
      return _minus(makesequence(args._VECTptr->back(),args._VECTptr->front()),contextptr);
    }
    return symbolic(at_complement,args);
  }
  static const char _complement_s []="complement";
  static define_unary_function_eval4 (__complement,&_complement,_complement_s,0,&texprintascomplement);
  define_unary_function_ptr5( at_complement ,alias_at_complement ,&__complement,0,true);

  gen and2intersect(const gen &g,GIAC_CONTEXT){
    return symbolic(at_intersect,g);
  }
  
  gen ou2union(const gen &g,GIAC_CONTEXT){
    return symbolic(at_union,g);
  }
  
  gen not2complement(const gen &g,GIAC_CONTEXT){
    return symbolic(at_complement,g);
  }
  
  gen xor2symdiff(const gen &g,GIAC_CONTEXT){
    return symbolic(at_symmetric_difference,g);
  }
  
  gen intersect2and(const gen &g,GIAC_CONTEXT){
    return symbolic(at_and,g);
  }
  
  gen union2ou(const gen &g,GIAC_CONTEXT){
    return symbolic(at_ou,g);
  }
  
  gen complement2not(const gen &g,GIAC_CONTEXT){
    return symbolic(at_not,g);
  }
  
  gen symdiff2xor(const gen &g,GIAC_CONTEXT){
    return symbolic(at_xor,g);
  }
  
  gen minus2andnot(const gen &g,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    return symbolic(at_and,makesequence(g._VECTptr->front(),symbolic(at_not,g._VECTptr->back())));
  }
  
  gen logic2set(const gen & g,GIAC_CONTEXT){
    vector<const unary_function_ptr *> vu;
    vu.push_back(at_and); 
    vu.push_back(at_ou); 
    vu.push_back(at_not); 
    vu.push_back(at_xor); 
    vector <gen_op_context> vv;
    vv.push_back(and2intersect);
    vv.push_back(ou2union);
    vv.push_back(not2complement);
    vv.push_back(xor2symdiff);
    return subst(g,vu,vv,false,contextptr);  
  }

  gen set2logic(const gen & g,GIAC_CONTEXT){
    vector<const unary_function_ptr *> vu;
    vu.push_back(at_intersect); 
    vu.push_back(at_union); 
    vu.push_back(at_complement); 
    vu.push_back(at_symmetric_difference); 
    vu.push_back(at_minus); 
    vector <gen_op_context> vv;
    vv.push_back(intersect2and);
    vv.push_back(union2ou);
    vv.push_back(complement2not);
    vv.push_back(symdiff2xor);
    vv.push_back(minus2andnot);
    return subst(g,vu,vv,false,contextptr);  
  }

  gen _logic2set(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_IDNT)
      return logic2set(eval(args,1,contextptr),contextptr);
    return logic2set(args,contextptr);
  }
  static const char _logic2set_s []="logic2set";
  static define_unary_function_eval (__logic2set,&_logic2set,_logic2set_s);
  define_unary_function_ptr5( at_logic2set ,alias_at_logic2set ,&__logic2set,_QUOTE_ARGUMENTS,true);

  gen _set2logic(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return set2logic(args,contextptr);
  }
  static const char _set2logic_s []="set2logic";
  static define_unary_function_eval (__set2logic,&_set2logic,_set2logic_s);
  define_unary_function_ptr5( at_set2logic ,alias_at_set2logic ,&__set2logic,0,true);

  static string texprintassymmdiff(const gen & g,const char * s,GIAC_CONTEXT){
    return texprintsommetasoperator(g,"\\Delta",contextptr);
  }
  gen symb_symmetric_difference(const gen & args){
    return symbolic(at_symmetric_difference,args);
  }
  gen _symmetric_difference(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ((args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gensizeerr();
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    interval2realset(a); interval2realset(b);
    if ( a.type==_VECT && b.type==_VECT){
      if (a.subtype==_REALSET__VECT || b.subtype==_REALSET__VECT){
        if (!convert_realset(a,contextptr) || !convert_realset(b,contextptr))
          return gensizeerr(contextptr);
        gen u=_union(makesequence(a,b),contextptr);
        gen i=_intersect(makesequence(a,b),contextptr);
        return _minus(makesequence(u,i),contextptr);
      }
      chk_set(a);
      chk_set(b);
      vecteur v;
      const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
      const_iterateur jt=b._VECTptr->begin(),jtend=b._VECTptr->end();
      for (;it!=itend && jt!=jtend;){
        if (*it==*jt){
          ++it; ++jt;
          continue;
        }
        if (set_sort(*it,*jt)){
          v.push_back(*it);
          ++it;
        }
        else {
          v.push_back(*jt);
          ++jt;
        }
      }
      for (;it!=itend;++it)
        v.push_back(*it);
      for (;jt!=jtend;++jt)
        v.push_back(*jt);
      return gen(v,_SET__VECT);
    }
    return symb_symmetric_difference(args); // gensizeerr(contextptr);
  }
  static const char _symmetric_difference_s []="symmetric_difference";
  static define_unary_function_eval4_index (178,__symmetric_difference,&_symmetric_difference,_symmetric_difference_s,&printsommetasoperator,&texprintassymmdiff);
  define_unary_function_ptr5( at_symmetric_difference ,alias_at_symmetric_difference ,&__symmetric_difference,0,T_UNION);

  gen symb_minus(const gen & args){
    return symbolic(at_minus,args);
  }
  gen _minus(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ((args.type!=_VECT) || (args._VECTptr->size()!=2))
      return symb_minus(args);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    interval2realset(a); interval2realset(b);
    if ( (a.type!=_VECT) || (b.type!=_VECT))
      return symb_minus(args);//gensizeerr(gettext("Minus"));
    if (a.subtype==_REALSET__VECT || b.subtype==_REALSET__VECT){
      if (!convert_realset(a,contextptr) || !convert_realset(b,contextptr))
        return gensizeerr(contextptr);
      gen bc=_complement(b,contextptr);
      return _intersect(makesequence(a,bc),contextptr);
    }
    chk_set(a);
    chk_set(b);
    vecteur v;
    const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
    const_iterateur jt=b._VECTptr->begin(),jtend=b._VECTptr->end();
    for (;it!=itend && jt!=jtend;){
      if (*it==*jt){
        ++it; ++jt;
      }
      else if (set_sort(*it,*jt)){
        v.push_back(*it);
        ++it;
      }
      else
        ++jt;
    }
    for (;it!=itend;++it)
      v.push_back(*it);
    return gen(v,_SET__VECT);
  }
  static const char _minus_s []=" minus ";
  static define_unary_function_eval4_index (60,__minus,&_minus,_minus_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_minus ,alias_at_minus ,&__minus);

  static string printasdollar(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_VECT)
      return sommetstr+feuille.print(contextptr);
    vecteur & v=*feuille._VECTptr;
    int s=int(v.size());
    if (s==2)
      return printsommetasoperator(feuille,sommetstr,contextptr);
    if (s==3)
      return v[0].print(contextptr)+sommetstr+v[1].print(contextptr)+" in "+v[2].print(contextptr);
    return gettext("error");
  }
  gen symb_dollar(const gen & args){
    return symbolic(at_dollar,args);
  }
  gen _dollar(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (abs_calc_mode(contextptr)==38){
      int r,c;
      if (iscell(args,r,c,contextptr)){
	string s=symbolic(at_dollar,args).print(contextptr);
	identificateur cellule(s);
	return eval(cellule,1,contextptr);
      }
      if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->back().type==_INT_ && args._VECTptr->back().val>0){
	string s=args._VECTptr->front().print(contextptr)+"$"+print_INT_(args._VECTptr->back().val);
	identificateur cellule(s);
	return eval(cellule,1,contextptr);
      }
    }
    vecteur vargs;
    if (args.type!=_VECT){
      identificateur tmp(" _t");
      vargs=makevecteur(tmp,symbolic(at_equal,gen(makevecteur(tmp,args),_SEQ__VECT)));
    }
    else
      vargs=*args._VECTptr;
    int s=int(vargs.size());
    if (s<2)
      return symb_dollar(args);
    gen a=vargs.front(),b=vargs[1],b1=eval(b,eval_level(contextptr),contextptr);
    if (b1.type==_VECT && b1.subtype==_SEQ__VECT && b1._VECTptr->size()==2){
      gen b11=b1._VECTptr->front();
      if (b.type==_VECT && b.subtype==_SEQ__VECT && b._VECTptr->size()==2 && b11.is_symb_of_sommet(at_equal) && b._VECTptr->front().is_symb_of_sommet(at_equal))
	b11=symb_equal(b._VECTptr->front()[1],b11[2]);
      return _dollar(gen(makevecteur(a,b11,b1._VECTptr->back()),_SEQ__VECT),contextptr);
    }
    if (a.is_symb_of_sommet(at_interval) && a._SYMBptr->feuille.type==_VECT && a._SYMBptr->feuille._VECTptr->size()==2){
      a=eval(a,1,contextptr);
      gen a1=a._SYMBptr->feuille._VECTptr->front(),a2=a._SYMBptr->feuille._VECTptr->back();
      gen nstep=(a2-a1)/b1;
      if (nstep.type==_DOUBLE_)
	nstep=(1+1e-12)*nstep;
      if (ck_is_positive(nstep,contextptr))
	nstep=_floor(nstep,contextptr);
      else {
	nstep=_floor(-nstep,contextptr);
	b1=-b1;
      }
      if (nstep.type!=_INT_ || nstep.val<0 || nstep.val>LIST_SIZE_LIMIT)
	return gendimerr(contextptr);
      vecteur res;
      for (unsigned i=0;int(i)<=nstep.val;++i){
	res.push_back(a1);
	a1 += b1;
      }
      return gen(res,_SEQ__VECT);
    }
    if (b1.type==_INT_ && b1.val>=0){
      if (b1.val>LIST_SIZE_LIMIT)
	return gendimerr(contextptr);
      return gen(vecteur(b1.val,eval(a,eval_level(contextptr),contextptr)),_SEQ__VECT);
    }
    gen var,intervalle,step=1;
    if ( (b.type==_SYMB) && (is_equal(b) || b._SYMBptr->sommet==at_same ) ){
      var=b._SYMBptr->feuille._VECTptr->front();
      if (var.type!=_IDNT)
	return gensizeerr(contextptr);
      /* Commented example seq(irem(g&^((p-1)/(Div[i])),p),i=(1 .. 2))
	 int status=*var._IDNTptr->quoted;
	 *var._IDNTptr->quoted=1;
	 a=eval(a,contextptr);
	 *var._IDNTptr->quoted=status;      
      */
      intervalle=eval(b._SYMBptr->feuille._VECTptr->back(),eval_level(contextptr),contextptr);
      if (s>=3)
	step=vargs[2];
    }
    else {
      if (s>=3){
	var=vargs[1];
	intervalle=eval(vargs[2],eval_level(contextptr),contextptr);
      }
      if (s>=4)
	step=vargs[3];
    }
    if (intervalle.type==_VECT){
      const_iterateur it=intervalle._VECTptr->begin(),itend=intervalle._VECTptr->end();
      vecteur res;
      for (;it!=itend;++it)
	res.push_back(eval(quotesubst(a,var,*it,contextptr),eval_level(contextptr),contextptr));
      return gen(res,_SEQ__VECT);
      // return gen(res,intervalle.subtype);
    }
    if ( (intervalle.type==_SYMB) && (intervalle._SYMBptr->sommet==at_interval)){
      gen c=intervalle._SYMBptr->feuille._VECTptr->front(),d=intervalle._SYMBptr->feuille._VECTptr->back();
      if ( (!is_integral(c) || !is_integral(d)) && abs_calc_mode(contextptr)==38)
	return gensizeerr(contextptr);
      gen debut=c,fin=d;
      bool reverse=ck_is_greater(debut,fin,contextptr);
      step=abs(step,contextptr);
      if (is_positive(abs(fin-debut,contextptr)-LIST_SIZE_LIMIT*step,contextptr))
	return gendimerr(contextptr);
      step=eval(reverse?-step:step,eval_level(contextptr),contextptr);
      vecteur res;
      for (;;debut+=step){
	if (ck_is_strictly_greater(reverse?fin:debut,reverse?debut:fin,contextptr))
	  break;
	res.push_back(eval(quotesubst(a,var,debut,contextptr),eval_level(contextptr),contextptr));
	if (debut==fin)
	  break;
      }
      return gen(res,_SEQ__VECT);
    }
    return symb_dollar(args);    
  }
  static const char _dollar_s []="$";
  string texprintasdollar(const gen & g,const char * s,GIAC_CONTEXT){
    if ( (g.type==_VECT) && (g._VECTptr->size()==2))
      return gen2tex(g._VECTptr->front(),contextptr)+"\\$"+gen2tex(g._VECTptr->back(),contextptr);
    return "\\$ "+g.print(contextptr);
  }
  static define_unary_function_eval4_index (125,__dollar,&_dollar,_dollar_s,&printasdollar,&texprintasdollar);
  define_unary_function_ptr5( at_dollar ,alias_at_dollar,&__dollar,_QUOTE_ARGUMENTS,0);

  static gen symb_makemat(const gen & args){
    return symbolic(at_makemat,args);
  }
  gen _makemat(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_INT_ && args.val>0 && double(args.val)*args.val<LIST_SIZE_LIMIT){
      vecteur res;
      for (int i=0;i<args.val;++i)
	res.push_back(vecteur(args.val,0));
      return gen(res,_MATRIX__VECT);
    }
    if (args.type!=_VECT)
      return symb_makemat(args);
    int s=int(args._VECTptr->size());
    if ( (s!=3) && (s!=2) )
      return symb_makemat(args);
    gen fonction,intervalle1,intervalle2;
    if (s==3){
      fonction=args._VECTptr->front();
      intervalle1=(*(args._VECTptr))[1];
      intervalle2=args._VECTptr->back();
    }
    else {
      intervalle1=args._VECTptr->front();
      intervalle2=args._VECTptr->back();
    }
    if (is_integral(intervalle1) && intervalle1.type==_INT_)
      intervalle1=symb_interval(makevecteur(zero,giacmax(intervalle1.val,1)-1));
    if (is_integral(intervalle2) && intervalle2.type==_INT_)
      intervalle2=symb_interval(makevecteur(zero,giacmax(intervalle2.val,1)-1));
    if ( (intervalle1.type!=_SYMB) || (intervalle1._SYMBptr->sommet!=at_interval) ||(intervalle2.type!=_SYMB) || (intervalle2._SYMBptr->sommet!=at_interval))
      return gensizeerr(gettext("makemat"));
    intervalle1=intervalle1._SYMBptr->feuille;
    intervalle2=intervalle2._SYMBptr->feuille;
    if ((intervalle1.type!=_VECT) || (intervalle1._VECTptr->size()!=2) || (intervalle2.type!=_VECT) || (intervalle2._VECTptr->size()!=2))
      return gensizeerr(gettext("interval"));
    gen debut_i=intervalle1._VECTptr->front(),fin_i=intervalle1._VECTptr->back();
    gen debut_j=intervalle2._VECTptr->front(),fin_j=intervalle2._VECTptr->back();
    if ( (debut_i.type!=_INT_) || (fin_i.type!=_INT_) || (debut_j.type!=_INT_) || (fin_j.type!=_INT_) )
      return gensizeerr(gettext("Boundaries not integer"));
    int di=debut_i.val,fi=fin_i.val,dj=debut_j.val,fj=fin_j.val;
    if (array_start(contextptr)){
      ++di; ++fi; ++dj; ++fj;
    }
    int stepi=1,stepj=1;
    if (di>fi)
      stepi=-1;
    if (dj>fj)
      stepj=-1;
    if ((fonction.type!=_SYMB) || (fonction._SYMBptr->sommet!=at_program)){
      int s=(fj-dj+1)*stepj;
      vecteur w(s,fonction);
      int t=(fi-di+1)*stepi;
      vecteur res(t);
      for (int i=0;i<t;++i)
	res[i]=w; // each element of res will be a free line, so that =< works
      return gen(res,_MATRIX__VECT);
    }
    vecteur v,w,a(2);
    v.reserve((fi-di)*stepi);
    w.reserve((fj-dj)*stepj);
    for (;;di+=stepi){
      a[0]=di;
      w.clear();
      for (int djj=dj;;djj+=stepj){
	a[1]=djj;
	w.push_back(fonction(gen(a,_SEQ__VECT),contextptr));
	if (djj==fj)
	  break;
      }
      v.push_back(w);
      if (di==fi)
	break;
    }
    return gen(v,_MATRIX__VECT);
  }
  static const char _makemat_s []="makemat";
  static define_unary_function_eval (__makemat,&_makemat,_makemat_s);
  define_unary_function_ptr5( at_makemat ,alias_at_makemat,&__makemat,0,true);

  gen symb_compose(const gen & args){
    return symbolic(at_compose,args);
  }
  gen _compose(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2&& args._VECTptr->back().type==_SPOL1){
      gen g=args._VECTptr->front();
      gen s=args._VECTptr->back();
      sparse_poly1 res;
      int shift=0;
      if (g.type==_SPOL1){
	vecteur v;
	if (sparse_poly12vecteur(*g._SPOL1ptr,v,shift))
	  g=v;
      }
      if (g.type==_VECT){
	vecteur v=*g._VECTptr;
	reverse(v.begin(),v.end());
	if (!pcompose(v,*s._SPOL1ptr,res,contextptr))
	  return sparse_poly1(1,monome(1,undef));
	if (shift==0)
	  return res;
	return spmul(res,sppow(*s._SPOL1ptr,shift,contextptr),contextptr);
      }
    }
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      gen g=args._VECTptr->front();
      gen s=args._VECTptr->back();
      if (g.type==_VECT && (g.subtype==_POLY1__VECT || s.type==_SPOL1 || (s.type==_VECT && s.subtype==_POLY1__VECT)) )
	return horner(*g._VECTptr,s,contextptr);
      if (ckmatrix(g) && ckmatrix(s))
	return g*s;
    }
    return symb_compose(args);
  }
  static const char _compose_s []="@";
  static define_unary_function_eval4 (__compose,&_compose,_compose_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_compose ,alias_at_compose ,&__compose);

  gen _composepow(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      gen base=args._VECTptr->front();
      gen expo=args._VECTptr->back();
      if (expo.type==_INT_){
	if (base.type==_SPOL1){
	  int n=expo.val;
	  sparse_poly1 s=*base._SPOL1ptr;
	  if (n<0){
	    gen tmp=_revert(s,contextptr);
	    if (tmp.type!=_SPOL1)
	      return tmp;
	    s=*tmp._SPOL1ptr;
	    n=-n;
	  }
	  gen res=sparse_poly1(1,monome(1,1)); // identity
	  for (;n>0;n/=2){
	    if (n%2) res=_compose(makesequence(res,s),contextptr);
	    gen tmp=_compose(makesequence(s,s),contextptr);
	    if (tmp.type!=_SPOL1)
	      return tmp;
	    s=*tmp._SPOL1ptr;
	  }
	  return res;
	}
	if (base.type==_VECT && base.subtype==_POLY1__VECT && expo.val>=0){
	  int n=expo.val;
	  vecteur v(2); v[0]=1;
	  gen res=gen(v,_POLY1__VECT);
	  gen s=base;
	  for (;n>0;n/=2){
	    if (n%2) res=horner(*res._VECTptr,s,contextptr);
	    s=horner(*s._VECTptr,s,contextptr);
	  }
	  return res;
	}
	if (ckmatrix(base))
	  return _pow(args,contextptr);
      }
    }
    return symbolic(at_composepow,args);
  }
  static const char _composepow_s []="@@";
  static define_unary_function_eval4 (__composepow,&_composepow,_composepow_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_composepow ,alias_at_composepow ,&__composepow);

  gen symb_args(const gen & args){
    return symbolic(at_args,args);
  }
  gen _args(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen e;
    if (debug_ptr(contextptr)->args_stack.empty())
      e=vecteur(0);
    else
      e=debug_ptr(contextptr)->args_stack.back();
    if ( (args.type==_VECT) && (args._VECTptr->empty()))
      return e;
    else
      return e(args,contextptr);
  }
  static const char _args_s []="args";
  static define_unary_function_eval (__args,&_args,_args_s);
  define_unary_function_ptr( at_args ,alias_at_args ,&__args);
  
  // static gen symb_lname(const gen & args){  return symbolic(at_lname,args);  }
  static void lidnt(const vecteur & v,vecteur & res,bool with_at){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      lidnt(*it,res,with_at);
  }

  extern const unary_function_ptr * const  at_int;

  void lidnt(const gen & args,vecteur & res,bool with_at){
    switch (args.type){
    case _IDNT:
      if (!equalposcomp(res,args))
	res.push_back(args);
      break;
    case _SYMB:
      if (with_at && args._SYMBptr->sommet==at_at){
	res.push_back(args);
	return;
      }
      if (args._SYMBptr->sommet==at_program && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==3){
	lidnt(args._SYMBptr->feuille._VECTptr->front(),res,with_at);
	lidnt(args._SYMBptr->feuille._VECTptr->back(),res,with_at);
	return;
      }
      if (args._SYMBptr->sommet==at_pnt && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==3){
	lidnt(args._SYMBptr->feuille._VECTptr->front(),res,with_at);
	lidnt((*args._SYMBptr->feuille._VECTptr)[1],res,with_at);
	return;
      }
      if ( (args._SYMBptr->sommet==at_integrate || args._SYMBptr->sommet==at_int || args._SYMBptr->sommet==at_sum) && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==4){
	vecteur & v =*args._SYMBptr->feuille._VECTptr;
	vecteur w(1,v[1]);
	lidnt(v[0],w,with_at);
	const_iterateur it=w.begin(),itend=w.end();
	for (++it;it!=itend;++it)
	  lidnt(*it,res,with_at);
	lidnt(v[2],res,with_at);
	lidnt(v.back(),res,with_at);
	return;
      }      
      lidnt(args._SYMBptr->feuille,res,with_at);
      break;
    case _VECT:
      lidnt(*args._VECTptr,res,with_at);
      break;
    }       
  }
  vecteur lidnt(const gen & args){
    vecteur res;
    lidnt(args,res,false);
    return res;
  }
  vecteur lidnt_with_at(const gen & args){
    vecteur res;
    lidnt(args,res,true);
    return res;
  }
  gen _lname(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    vecteur res=makevecteur(cst_pi,cst_euler_gamma);
    lidnt(args,res,false);
    return vecteur(res.begin()+2,res.end());
  }
  static const char _lname_s []="lname";
  static define_unary_function_eval (__lname,&_lname,_lname_s);
  define_unary_function_ptr5( at_lname ,alias_at_lname,&__lname,0,true);

  static gen symb_has(const gen & args){
    return symbolic(at_has,args);
  }
  gen _has(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2))
      return symb_has(args);
    gen tmp(_lname(args._VECTptr->front(),contextptr));
    if (tmp.type!=_VECT) return tmp;
    return equalposcomp(*tmp._VECTptr,args._VECTptr->back());
  }
  static const char _has_s []="has";
  static define_unary_function_eval (__has,&_has,_has_s);
  define_unary_function_ptr5( at_has ,alias_at_has,&__has,0,true);

  gen _kill(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->empty()){
      if (!contextptr)
	protection_level=0;
      debug_ptr(contextptr)->debug_mode=false;
      debug_ptr(contextptr)->current_instruction_stack.clear();
      debug_ptr(contextptr)->sst_at_stack.clear();
      debug_ptr(contextptr)->args_stack.clear();
      return gensizeerr(gettext("Program killed"));
    }
#ifdef HAVE_LIBPTHREAD
    if (args.type==_VECT)
      return apply(args,_kill,contextptr);
    if (args.type==_POINTER_ && args.subtype==_THREAD_POINTER){
      context * cptr=(context *) args._POINTER_val;
      thread_param * tptr =thread_param_ptr(cptr);
      if (cptr 
#ifndef __MINGW_H
	  && tptr->eval_thread
#endif
	  ){
	gen g=tptr->v[0];
	if (g.type==_VECT && g._VECTptr->size()==2 && g._VECTptr->front().is_symb_of_sommet(at_quote)){
	  pthread_mutex_lock(cptr->globalptr->_mutex_eval_status_ptr);
	  gen tmpsto=sto(undef,g._VECTptr->front()._SYMBptr->feuille,cptr);
	  if (is_undef(tmpsto)) return tmpsto;
	  pthread_mutex_unlock(cptr->globalptr->_mutex_eval_status_ptr);
	}
      }
      kill_thread(1,cptr);
      return 1;
    }
#endif
    return gentypeerr(contextptr);
  }
  static const char _kill_s []="kill";
  static define_unary_function_eval (__kill,&_kill,_kill_s);
  define_unary_function_ptr5( at_kill ,alias_at_kill,&__kill,0,true);

  gen _halt(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_mode=true;
      return plus_one;
    }
    return zero;
  }
  static const char _halt_s []="halt";
  static define_unary_function_eval_quoted (__halt,&_halt,_halt_s);
  define_unary_function_ptr5( at_halt ,alias_at_halt,&__halt,_QUOTE_ARGUMENTS,true);

  gen _debug(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
#ifdef EMCC2
#ifdef HAVE_LIBFLTK
    *logptr(contextptr) << "Hint: run debug from Prg menu for a better user interface\n";
#endif
#else
    if (child_id && thread_eval_status(contextptr)!=1)
      return args;
#endif
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_in_mode=true;
      debug_ptr(contextptr)->debug_prog_name=0;
    }
    return args.eval(eval_level(contextptr),contextptr);
  }
  static const char _debug_s []="debug";
  static define_unary_function_eval_quoted (__debug,&_debug,_debug_s);
  define_unary_function_ptr5( at_debug ,alias_at_debug,&__debug,_QUOTE_ARGUMENTS,true);

  gen _sst_in(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id)
      return zero;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_in_mode=true;
      return plus_one;
    }
    return zero;
  }
  static const char _sst_in_s []="sst_in";
  static define_unary_function_eval_quoted (__sst_in,&_sst_in,_sst_in_s);
  define_unary_function_ptr5( at_sst_in ,alias_at_sst_in,&__sst_in,_QUOTE_ARGUMENTS,true);

  gen _sst(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id)
      return args;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->debug_mode=true;
      debug_ptr(contextptr)->sst_mode=true;
      return plus_one;
    }
    return zero;
  }
  static const char _sst_s []="sst";
  static define_unary_function_eval_quoted (__sst,&_sst,_sst_s);
  define_unary_function_ptr5( at_sst ,alias_at_sst,&__sst,_QUOTE_ARGUMENTS,true);

  gen _cont(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id)
      return args;
    if (debug_ptr(contextptr)->debug_allowed){
      debug_ptr(contextptr)->sst_mode=false;
      return plus_one;
    }
    return zero;
  }
  static const char _cont_s []="cont";
  static define_unary_function_eval_quoted (__cont,&_cont,_cont_s);
  define_unary_function_ptr5( at_cont ,alias_at_cont,&__cont,_QUOTE_ARGUMENTS,true);

  static gen watch(const gen & args,GIAC_CONTEXT){
    if (!equalposcomp(debug_ptr(contextptr)->debug_watch,args))
      debug_ptr(contextptr)->debug_watch.push_back(args);
    return args;
  }
  gen _watch(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id && thread_eval_status(contextptr)!=1 )
      return args;
    if (args.type==_VECT && args._VECTptr->empty() && debug_ptr(contextptr)->debug_localvars)
      apply( *debug_ptr(contextptr)->debug_localvars,contextptr,watch);
    else
      apply(args,contextptr,watch);
    return debug_ptr(contextptr)->debug_watch;
  }
  static const char _watch_s []="watch";
  static define_unary_function_eval_quoted (__watch,&_watch,_watch_s);
  define_unary_function_ptr5( at_watch ,alias_at_watch,&__watch,_QUOTE_ARGUMENTS,true);

  static gen rmwatch(const gen & args,GIAC_CONTEXT){
    int pos;
    if (args.type==_INT_){
      pos=args.val+1;
      if (pos>signed(debug_ptr(contextptr)->debug_watch.size()))
	return debug_ptr(contextptr)->debug_watch;
    }
    else 
      pos=equalposcomp(debug_ptr(contextptr)->debug_watch,args);
    if (pos){
      debug_ptr(contextptr)->debug_watch.erase(debug_ptr(contextptr)->debug_watch.begin()+pos-1,debug_ptr(contextptr)->debug_watch.begin()+pos);
      return debug_ptr(contextptr)->debug_watch;
    }
    return zero;
  }

  gen _rmwatch(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->empty() && debug_ptr(contextptr)->debug_localvars)
      return apply( *debug_ptr(contextptr)->debug_localvars,contextptr,rmwatch);
    else
      return apply(args,contextptr,rmwatch);
  }
  static const char _rmwatch_s []="rmwatch";
  static define_unary_function_eval_quoted (__rmwatch,&_rmwatch,_rmwatch_s);
  define_unary_function_ptr5( at_rmwatch ,alias_at_rmwatch,&__rmwatch,_QUOTE_ARGUMENTS,true);

  gen _breakpoint(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id && thread_eval_status(contextptr)!=1)
      return args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) || (args._VECTptr->front().type!=_IDNT) || (args._VECTptr->back().type!=_INT_) )
      return zero;
    if (!equalposcomp(debug_ptr(contextptr)->debug_breakpoint,args)){
      debug_ptr(contextptr)->debug_breakpoint.push_back(args);
      // FIXME should also modify debug_ptr(contextptr)->sst_at_stack if the breakpoint applies
      // to a program != current program
      if (!debug_ptr(contextptr)->args_stack.empty() && debug_ptr(contextptr)->args_stack.back().type==_VECT && debug_ptr(contextptr)->args_stack.back()._VECTptr->front()==args._VECTptr->front())
	debug_ptr(contextptr)->sst_at.push_back(args._VECTptr->back().val);
    }
    return debug_ptr(contextptr)->debug_breakpoint;
  }
  static const char _breakpoint_s []="breakpoint";
  static define_unary_function_eval_quoted (__breakpoint,&_breakpoint,_breakpoint_s);
  define_unary_function_ptr5( at_breakpoint ,alias_at_breakpoint,&__breakpoint,_QUOTE_ARGUMENTS,true);

  gen _rmbreakpoint(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (child_id&& thread_eval_status(contextptr)!=1)
      return args;
    int pos;
    if (args.type==_INT_){
      pos=args.val;
      if (pos<1 || pos>signed(debug_ptr(contextptr)->debug_breakpoint.size())){
	adjust_sst_at(*debug_ptr(contextptr)->debug_prog_name,contextptr);
	return debug_ptr(contextptr)->debug_breakpoint;
      }
    }
    else 
      pos=equalposcomp(debug_ptr(contextptr)->debug_breakpoint,args);
    if (pos){
      debug_ptr(contextptr)->debug_breakpoint.erase(debug_ptr(contextptr)->debug_breakpoint.begin()+pos-1,debug_ptr(contextptr)->debug_breakpoint.begin()+pos);
      adjust_sst_at(*debug_ptr(contextptr)->debug_prog_name,contextptr);
      return debug_ptr(contextptr)->debug_breakpoint;
    }
    return zero;
  }
  static const char _rmbreakpoint_s []="rmbreakpoint";
  static define_unary_function_eval_quoted (__rmbreakpoint,&_rmbreakpoint,_rmbreakpoint_s);
  define_unary_function_ptr5( at_rmbreakpoint ,alias_at_rmbreakpoint,&__rmbreakpoint,_QUOTE_ARGUMENTS,true);

#ifdef KHICAS
#ifdef NSPIRE_NEWLIB
#define lineh 14
#else
#define lineh 12
#endif
  void debug_loop(gen &res,GIAC_CONTEXT){
    if (!debug_ptr(contextptr)->debug_allowed || (!debug_ptr(contextptr)->sst_mode && !equalposcomp(debug_ptr(contextptr)->sst_at,debug_ptr(contextptr)->current_instruction)) )
      return;
    // Fill dbgptr->debug_info_ptr and fast_debug_info_ptr 
    // with debugging infos to be displayed
    debug_struct * dbgptr=debug_ptr(contextptr);
    vecteur w;
    string progs;
    // w[0]=function, args,
    // w[1]=breakpoints
    // w[2] = instruction to eval or program if debugging a prog
    // w[3]= evaluation result
    // w[4]= current instruction number 
    // w[5] = watch vector, w[6] = watch values
    if (!debug_ptr(contextptr)->args_stack.empty()){
      w.push_back(debug_ptr(contextptr)->args_stack.back());
      w.push_back(vector_int_2_vecteur(debug_ptr(contextptr)->sst_at,contextptr));
    }
    else {
      w.push_back(undef);
      w.push_back(undef);
    }
    gen w2=(*debug_ptr(contextptr)->fast_debug_info_ptr);
    if (w2.type==_VECT && w2._VECTptr->size()>3)
      w2=w2[2];
    //*logptr(contextptr) << w2 << endl;
    w.push_back(w2);
    w.push_back(res);
    w.push_back(debug_ptr(contextptr)->current_instruction);
    vecteur dw=debug_ptr(contextptr)->debug_watch;
    if (contextptr && dw.empty()){
      // put the last 2 environments
      const context * cur=contextptr;
      sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
      for (;it!=itend;++it){
	dw.push_back(identificateur(it->first));
      }
      if (cur->previous && cur->previous!=cur->globalcontextptr){
	cur=cur->previous;
	sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
	for (;it!=itend;++it){
	  dw.push_back(identificateur(it->first));
	}
      }
    }
    w.push_back(dw);
    os_fill_rect(0,0,LCD_WIDTH_PX,LCD_HEIGHT_PX,_WHITE);
    int dispx=0,dispy=12;
    // print debugged program instructions from current-2 to current+3
    progs="debug "+w[0].print(contextptr)+'\n';
    if (w[4].type==_INT_){
      vector<string> ws;
#if 1
      ws.push_back("");
      debug_print(w[2],ws,contextptr);
#else
      int l=s.size();
      string cur;
      for (int i=0;i<l;++i){
	if (s[i]=='\n'){
	  ws.push_back(cur);
	  cur="";
	}
	else cur+=s[i];
      }
      ws.push_back(cur);
#endif
      int m=giacmax(0,w[4].val-2),M=giacmin(w[4].val+3,ws.size()-1);
      if (M-m<5)
	M=giacmin(m+5,ws.size()-1);
      if (M-m<5)
	m=giacmax(0,M-5);
      for (int i=m;i<=M;++i){
	os_draw_string_small(dispx,dispy,(i==w[4].val?_WHITE:_BLACK),(i==w[4].val?_BLACK:_WHITE),(print_INT_(i)+":"+ws[i]).c_str());
	//mPrintXY(dispx,dispy,(print_INT_(i)+":"+ws[i]).c_str(),(i==w[4].val?TEXT_MODE_INVERT:TEXT_MODE_TRANSPARENT_BACKGROUND),TEXT_COLOR_BLACK);
	dispy+=lineh;
	dispx=0;
	// progs += print_INT_(i)+((i==w[4].val)?" => ":"    ")+ws[i]+'\n';
      }
    }
    else {
      string s=w[2].print(contextptr);
      progs += "\nprg: "+s+" # "+w[4].print(contextptr);
    }
    os_draw_string_small_(dispx,dispy,"----------------");
    dispx=0;
    dispy += lineh-3;
    // progs += "======\n";
    // evaluate watch with debug_ptr(contextptr)->debug_allowed=false
    debug_ptr(contextptr)->debug_allowed=false;
    string evals,breaks;
    iterateur it=dw.begin(),itend=dw.end();
    int nvars=itend-it;
    bool fewvars=nvars<7;
    for (int nv=0;it!=itend;++nv,++it){
      evals += it->print(contextptr)+"=";
      gen tmp=protecteval(*it,1,contextptr);
      string s=tmp.print(contextptr);
      if (!fewvars && s.size()>37)
	s=s.substr(0,35)+"...";
      evals += s+",";
      if (fewvars || (nv % 2)==1 || nv==nvars-1){
	os_draw_string_small_(dispx,dispy,evals.c_str());
	dispy+=lineh;
	evals="";
	// evals += '\n';
      }
      else
	evals += "    ";
    }
    if (evals.size()!=0)
      os_draw_string_small_(dispx,dispy,evals.c_str());
    dispx=0;
    dispy=LCD_HEIGHT_PX-18;
    os_draw_string_small_(dispx,dispy,"down: next, right: in, EXE: cont. EXIT: kill");
    w.push_back(dw);
    debug_ptr(contextptr)->debug_allowed=true;
    *dbgptr->debug_info_ptr=w;
    while (1){
      clear_abort();
      int key;
      GetKey(&key);
      set_abort();
      // convert key to i
      int i=0;
      if (key==KEY_CTRL_DOWN || key==KEY_CTRL_OK){
	i=-1;
      }
      if (key==KEY_CTRL_RIGHT){
	i=-2;
      }
      if (key==KEY_CTRL_EXE){
	i=-3;
      }
      if (key==KEY_CTRL_EXIT){
	i=-4;
      }
      if (key==KEY_CTRL_F5){
	i=-5;
      }
      if (key==KEY_CTRL_F6){
	i=-6;
      }
      if (i>0){
	gen tmp=i;
	if (tmp.is_symb_of_sommet(at_equal))
	  tmp=equaltosto(tmp,contextptr);
	evals=string("eval: ")+tmp.print(contextptr)+" => "+protecteval(tmp,1,contextptr).print(contextptr)+'\n';
	cout << evals ;
	iterateur it=dw.begin(),itend=dw.end();
	for (;it!=itend;++it){
	  evals += it->print(contextptr)+"=";
	  gen tmp=protecteval(*it,1,contextptr);
	  evals += tmp.print(contextptr)+",";
	}
      }
      // CERR << i << endl;
      if (i==-1){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=true;
	os_hide_graph();
	return;
      }
      if (i==-2){
	dbgptr->sst_in_mode=true;
	dbgptr->sst_mode=true;
	os_hide_graph();
	return;
      }
      if (i==-3){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=false;
	os_hide_graph();
	return;
      }
      if (i==-4){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=false;
	//debug_ptr(contextptr)->current_instruction_stack.clear();
	//debug_ptr(contextptr)->sst_at_stack.clear();
	//debug_ptr(contextptr)->args_stack.clear();
	ctrl_c=interrupted=true;
	os_hide_graph();
	return;
      }
      if (i==-5){
	breaks="break line "+print_INT_(w[4].val)+'\n';
	_breakpoint(makesequence(w[0][0],w[4]),contextptr);
      }
      if (i==-6){
	breaks="remove break line "+print_INT_(w[4].val)+'\n';
	_rmbreakpoint(makesequence(w[0][0],w[4]),contextptr);
      }
    } // end while(1)
  }

#else // KHICAS

#if (defined EMCC || defined EMCC2 ) && !defined GIAC_GGB
  void debug_loop(gen &res,GIAC_CONTEXT){
    if (!debug_ptr(contextptr)->debug_allowed || (!debug_ptr(contextptr)->sst_mode && !equalposcomp(debug_ptr(contextptr)->sst_at,debug_ptr(contextptr)->current_instruction)) )
      return;
    // Fill dbgptr->debug_info_ptr and fast_debug_info_ptr 
    // with debugging infos to be displayed
    debug_struct * dbgptr=debug_ptr(contextptr);
    vecteur w;
    string progs;
    // w[0]=function, args,
    // w[1]=breakpoints
    // w[2] = instruction to eval or program if debugging a prog
    // w[3]= evaluation result
    // w[4]= current instruction number 
    // w[5] = watch vector, w[6] = watch values
    if (!debug_ptr(contextptr)->args_stack.empty()){
      w.push_back(debug_ptr(contextptr)->args_stack.back());
      w.push_back(vector_int_2_vecteur(debug_ptr(contextptr)->sst_at,contextptr));
    }
    else {
      w.push_back(undef);
      w.push_back(undef);
    }
    gen w2=(*debug_ptr(contextptr)->fast_debug_info_ptr);
    if (w2.type==_VECT && w2._VECTptr->size()>3)
      w2=w2[2];
    //*logptr(contextptr) << w2 << '\n';
    w.push_back(w2);
    w.push_back(res);
    w.push_back(debug_ptr(contextptr)->current_instruction);
    vecteur dw=debug_ptr(contextptr)->debug_watch;
    if (contextptr && dw.empty()){
      // put the last 2 environments
      const context * cur=contextptr;
      sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
      for (;it!=itend;++it){
	dw.push_back(identificateur(it->first));
      }
      if (cur->previous && cur->previous!=cur->globalcontextptr){
	cur=cur->previous;
	sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
	for (;it!=itend;++it){
	  dw.push_back(identificateur(it->first));
	}
      }
    }
    w.push_back(dw);
#if defined EMCC2 && defined HAVE_LIBFLTK
    static  Fl_Hold_Browser * prgsrc_browser=0,*var_browser=0;
    static Fl_Button * button1=0,*button2=0,*button3=0,*button4=0;
    if (emfltkdbg){
      if (emdbg_w==0){
	Fl_Group::current(xcas::Xcas_Main_Window);
	int dx=500,dy=400,L=xcas::Xcas_MainTab->labelsize();
	emdbg_w=new Fl_Group(0,0,dx,dy);
	prgsrc_browser = new Fl_Hold_Browser(2,2,dx-2,dy/2-(L+4));
	prgsrc_browser->format_char(0);
	prgsrc_browser->type(2);
	prgsrc_browser->label(gettext("Source"));
	prgsrc_browser->align(FL_ALIGN_TOP);
	prgsrc_browser->labeltype(FL_NO_LABEL);
	int ypos=prgsrc_browser->y()+prgsrc_browser->h()+2;
	button1 = new Fl_Button(2,ypos,dx/4-4,L+2);
	button1->shortcut(0xff0d);
	button1->label(gettext("sst"));
	button1->tooltip(gettext("Click to execute next step"));
	button2 = new Fl_Button(dx/4,ypos,dx/4-4,L+2);
	button2->label(gettext("in"));
	button2->tooltip(gettext("Click to step in function"));
	button3 = new Fl_Button(2*dx/4+2,ypos,dx/4-4,L+2);
	button3->label(gettext("cont"));
	button3->tooltip(gettext("Click to continue non stop"));
	button4 = new Fl_Button(3*dx/4+2,ypos,dx/4-4,L+2);
	button4->shortcut(0xff1b);
	button4->label(gettext("Click to cancel"));
	ypos += L+4;
	var_browser = new Fl_Hold_Browser(prgsrc_browser->x(),ypos,prgsrc_browser->w(),prgsrc_browser->h());
	var_browser->label("Local variables");
	var_browser->type(2);
	var_browser->align(FL_ALIGN_TOP);
	var_browser->labeltype(FL_NO_LABEL);
	emdbg_w->end();
	emdbg_w->resizable(emdbg_w);
	Fl_Group::current(0);
      }
      prgsrc_browser->clear();
      var_browser->clear();
      if (w[4].type==_INT_){
	vector<string> ws;
	ws.push_back("");
	debug_print(w[2],ws,contextptr);
	// int m=giacmax(0,w[4].val-2),M=giacmin(w[4].val+3,ws.size()-1);
	int m=0,M=ws.size()-1;
	for (int i=m;i<=M;++i){
	  prgsrc_browser->add((print_INT_(i)+((i==w[4].val)?" => ":"    ")+ws[i]).c_str());
	}
	prgsrc_browser->value(w[4].val-m+1);
      }
      else {
	string s=w[2].print(contextptr);
	progs += "\nprg: "+s+" # "+w[4].print(contextptr);
	prgsrc_browser->add(progs.c_str());
      }
    }
    else 
#endif
      {
	// print debugged program instructions from current-2 to current+3
	progs="debug "+w[0].print(contextptr)+'\n';
	if (w[4].type==_INT_){
	  vector<string> ws;
	  ws.push_back("");
	  debug_print(w[2],ws,contextptr);
	  int m=giacmax(0,w[4].val-2),M=giacmin(w[4].val+3,ws.size()-1);
	  for (int i=m;i<=M;++i){
	    progs += print_INT_(i)+((i==w[4].val)?" => ":"    ")+ws[i]+'\n';
	  }
	}
	else {
	  string s=w[2].print(contextptr);
	  progs += "\nprg: "+s+" # "+w[4].print(contextptr);
	}
	progs += "======\n";
      }
    // evaluate watch with debug_ptr(contextptr)->debug_allowed=false
    debug_ptr(contextptr)->debug_allowed=false;
    string evals,breaks;
    iterateur it=dw.begin(),itend=dw.end();
    int nvars=itend-it;
    for (int nv=0;it!=itend;++nv,++it){
      evals += it->print(contextptr)+"=";
      gen tmp=protecteval(*it,1,contextptr);
      string s=tmp.print(contextptr);
      if (s.size()>100) s=s.substr(0,97)+"...";
      evals += s;
#if defined EMCC2 && defined HAVE_LIBFLTK
      if (emfltkdbg){
	var_browser->add(evals.c_str());
	evals="";
      } else
#endif
	{
	  evals += ",";
	  if (nvars<4 || (nv % 2)==1 || nv==nvars-1)
	    evals += '\n';
	  else
	    evals += "    ";
	}
    }
    w.push_back(dw);
    debug_ptr(contextptr)->debug_allowed=true;
    *dbgptr->debug_info_ptr=w;
#if defined EMCC2 && defined HAVE_LIBFLTK
    if (emfltkdbg){
      Fl_Window * mainw=xcas::Xcas_Main_Window;
      if (mainw){
	int X=xcas::Xcas_MainTab->x(),Y=xcas::Xcas_MainTab->y(),W=xcas::Xcas_MainTab->w(),H=xcas::Xcas_MainTab->h();
	if (xcas::Xcas_MainTab){
	  emdbg_w->resize(X,Y,W,H);
	  xcas::change_group_fontsize(emdbg_w,xcas::Xcas_MainTab->labelsize());
	  xcas::Xcas_MainTab->hide();
	  emdbg_w->show();
	}
      }
      int r=0;
      for (;;) {
	Fl_Widget *o = Fl::readqueue();
	if (!o){
	  if (xcas::Xcas_Main_Window)
	    Xcas_emscripten_main_loop();
	  else
	    Fl::wait();
	}
	else {
	  if (o == button1) {r = -1; break;} // sst
	  if (o == button2) {r = -2; break;} // in
	  if (o == button3) {r = -3; break;} // cont
	  if (o == button4) {r = -4; break;} // kill
	}
      }
      if (xcas::Xcas_MainTab) xcas::Xcas_MainTab->show();
      emdbg_w->hide();
      if (r==-1){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=true;
	return;
      }
      if (r==-2){
	dbgptr->sst_in_mode=true;
	dbgptr->sst_mode=true;
	return;
      }
      if (r==-3){
	dbgptr->sst_in_mode=false;
	dbgptr->sst_mode=false;
	return;
      }
      if (r==-4){
	if (!contextptr)
	  protection_level=0;
	debug_ptr(contextptr)->debug_mode=false;
	debug_ptr(contextptr)->current_instruction_stack.clear();
	debug_ptr(contextptr)->sst_at_stack.clear();
	debug_ptr(contextptr)->args_stack.clear();
	ctrl_c=interrupted=true;
	return;
      }
      return ; // should never be reached
    } else
#endif // EMCC2 && FLTK
      {
	// dbgptr->debug_refresh=false;
	// need a way to pass w to EM_ASM like environment and call HTML5 prompt
	while (1){
	  int i=EM_ASM_INT({
	      var msg = UTF8ToString($0);//Pointer_stringify($0); // Convert message to JS string
	      var tst=prompt(msg,'n');             // Use JS version of alert
	      if (tst==null) return -4;
	      if (tst=='next' || tst=='n' || tst=='sst') return -1;
	      if (tst=='sst_in' || tst=='s' ) return -2;
	      if (tst=='cont' || tst=='c' ) return -3;
	      if (tst=='kill' || tst=='k' ) return -4;
	      if (tst=='break' || tst=='b' ) return -5;
	      if (tst=='delete' || tst=='d' ) return -6;
	      return allocate(intArrayFromString(tst), 'i8', ALLOC_NORMAL);
	    }, (progs+breaks+evals+"======\nn: next, s:step in, c:cont, b: break, k: kill, d:del").c_str());
	  breaks="";
	  if (i>0){
	    char *ptr=(char *)i;
	    gen tmp=gen(ptr,contextptr);
	    free(ptr);
	    if (tmp.is_symb_of_sommet(at_equal)) tmp=equaltosto(tmp,contextptr);
	    evals=(string("eval: ")+ptr)+" => "+protecteval(tmp,1,contextptr).print(contextptr)+'\n';
	    CERR << evals ;
	    iterateur it=dw.begin(),itend=dw.end();
	    for (;it!=itend;++it){
	      evals += it->print(contextptr)+"=";
	      gen tmp=protecteval(*it,1,contextptr);
	      evals += tmp.print(contextptr)+",";
	    }
	  }
	  // CERR << i << '\n';
	  if (i==-1){
	    dbgptr->sst_in_mode=false;
	    dbgptr->sst_mode=true;
	    return;
	  }
	  if (i==-2){
	    dbgptr->sst_in_mode=true;
	    dbgptr->sst_mode=true;
	    return;
	  }
	  if (i==-3){
	    dbgptr->sst_in_mode=false;
	    dbgptr->sst_mode=false;
	    return;
	  }
	  if (i==-4){
	    if (!contextptr)
	      protection_level=0;
	    debug_ptr(contextptr)->debug_mode=false;
	    debug_ptr(contextptr)->current_instruction_stack.clear();
	    debug_ptr(contextptr)->sst_at_stack.clear();
	    debug_ptr(contextptr)->args_stack.clear();
	    ctrl_c=interrupted=true;
	    return;
	  }
	  if (i==-5){
	    breaks="break line "+print_INT_(w[4].val)+'\n';
	    _breakpoint(makesequence(w[0][0],w[4]),contextptr);
	  }
	  if (i==-6){
	    breaks="remove break line "+print_INT_(w[4].val)+'\n';
	    _rmbreakpoint(makesequence(w[0][0],w[4]),contextptr);
	  }
	} // end while(i>0)
      }
  }
#else // EMCC
  
#ifdef GIAC_HAS_STO_38
  void aspen_debug_loop(gen & res,GIAC_CONTEXT);
  
  void debug_loop(gen &res,GIAC_CONTEXT){
    aspen_debug_loop(res,contextptr);
  }

#else // GIAC_HAS_STO_38

  void debug_loop(gen &res,GIAC_CONTEXT){
    if (!debug_ptr(contextptr)->debug_allowed || (!debug_ptr(contextptr)->sst_mode && !equalposcomp(debug_ptr(contextptr)->sst_at,debug_ptr(contextptr)->current_instruction)) )
      return;
    // Detect thread debugging
    int thread_debug=thread_eval_status(contextptr);
    if (thread_debug>1)
      return;
    if (thread_debug==1){
      // Fill dbgptr->debug_info_ptr and fast_debug_info_ptr 
      // with debugging infos to be displayed
      debug_struct * dbgptr=debug_ptr(contextptr);
      vecteur w; 
      // w[0]=function, args,
      // w[1]=breakpoints
      // w[2] = instruction to eval or program if debugging a prog
      // w[3]= evaluation result
      // w[4]= current instruction number 
      // w[5] = watch vector, w[6] = watch values
      if (!debug_ptr(contextptr)->args_stack.empty()){
	w.push_back(debug_ptr(contextptr)->args_stack.back());
	w.push_back(vector_int_2_vecteur(debug_ptr(contextptr)->sst_at,contextptr));
      }
      else {
	w.push_back(undef);
	w.push_back(undef);
      }
      w.push_back((*debug_ptr(contextptr)->fast_debug_info_ptr));
      w.push_back(res);
      w.push_back(debug_ptr(contextptr)->current_instruction);
      vecteur dw=debug_ptr(contextptr)->debug_watch;
      if (contextptr && dw.empty()){
	// put the last 2 environments
	const context * cur=contextptr;
	sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
	for (;it!=itend;++it){
	  dw.push_back(identificateur(it->first));
	}
	if (cur->previous && cur->previous!=cur->globalcontextptr){
	  cur=cur->previous;
	  sym_tab::const_iterator it=cur->tabptr->begin(),itend=cur->tabptr->end();
	  for (;it!=itend;++it){
	    dw.push_back(identificateur(it->first));
	  }
	}
      }
      w.push_back(dw);
      // evaluate watch with debug_ptr(contextptr)->debug_allowed=false
      debug_ptr(contextptr)->debug_allowed=false;
      iterateur it=dw.begin(),itend=dw.end();
      for (;it!=itend;++it)
	*it=protecteval(*it,1,contextptr);
      w.push_back(dw);
      debug_ptr(contextptr)->debug_allowed=true;
      *dbgptr->debug_info_ptr=w;
      dbgptr->debug_refresh=false;
      // Switch to level 2, waiting for main
      thread_eval_status(2,contextptr);
      for (;;){
	// Wait until status is put back by main to level 1
	wait_1ms(10);
	if (thread_eval_status(contextptr)==1){
	  // the wait function of the main thread should put in debug_info_ptr
	  // the next instruction, here we check for sst/sst_in/cont/kill
	  if (dbgptr->fast_debug_info_ptr){
	    gen test=*dbgptr->fast_debug_info_ptr;
	    if (test.type==_SYMB)
	      test=test._SYMBptr->sommet;
	    if (test.type==_FUNC){
	      if (test==at_sst){
		dbgptr->sst_in_mode=false;
		dbgptr->sst_mode=true;
		return;
	      }
	      if (test==at_sst_in){
		dbgptr->sst_in_mode=true;
		dbgptr->sst_mode=true;
		return;
	      }
	      if (test==at_cont){
		dbgptr->sst_in_mode=false;
		dbgptr->sst_mode=false;
		return;
	      }
	      if (test==at_kill){
		_kill(0,contextptr);
		return;
	      }
	    } // end type _FUNC
	    // eval
	    w[2] = *dbgptr->fast_debug_info_ptr;
	    w[3] = *dbgptr->fast_debug_info_ptr = protecteval(w[2],1,contextptr);
	    *dbgptr->debug_info_ptr=w;
	    dbgptr->debug_refresh=true;
	  } // end if (*dbgptr->debug_info_ptr)
	  thread_eval_status(2,contextptr); // Back to level 2
	} // end if (thread_eval_status()==1)
      } // end '\n'ess for loop
    } // end thread debugging
#if (defined WIN32) || (!defined HAVE_SIGNAL_H_OLD)
    *logptr(contextptr) << gettext("Sorry! Debugging requires a true operating system") << '\n';
    *logptr(contextptr) << gettext("Please try xcas on Linux or an Unix") << '\n';
    return;
#else // WIN32
    if (child_id)
      return;
    vecteur w; 
    // w[0]=[function + args, breakpoints]
    // w[2]= res of last evaluation, 
    // w[3] = next instruction, w[4]=debug_ptr(contextptr)->current_instruction
    // w[5] = watch vector, w[6] = watch values
    // evaluate watch with debug_ptr(contextptr)->debug_allowed=false
    debug_ptr(contextptr)->debug_allowed=false;
    debug_ptr(contextptr)->debug_allowed=true;
    if (!debug_ptr(contextptr)->args_stack.empty()){
      w.push_back(makevecteur(debug_ptr(contextptr)->args_stack.back(),vector_int_2_vecteur(debug_ptr(contextptr)->sst_at,contextptr)));
    }
    else
      w.push_back(undef);
    w.push_back(undef);
    w.push_back(res);
    w.push_back((*debug_ptr(contextptr)->fast_debug_info_ptr));
    (*debug_ptr(contextptr)->fast_debug_info_ptr)=undef;
    w.push_back(debug_ptr(contextptr)->current_instruction);
    w.push_back(debug_ptr(contextptr)->debug_watch);
    w.push_back(undef);
    bool in_debug_loop=true;
    for (;in_debug_loop;){
#ifndef NO_STDEXCEPT
      try {
#endif
	vecteur tmp=gen2vecteur(debug_ptr(contextptr)->debug_watch);
	iterateur it=tmp.begin(),itend=tmp.end();
	for (;it!=itend;++it)
	  *it=it->eval(1,contextptr);
	w[6]=tmp;
#ifndef NO_STDEXCEPT
      }
      catch (std::runtime_error & error){
	last_evaled_argptr(contextptr)=NULL;
	w[6]=string2gen(error.what(),false);
      }
#endif
      ofstream child_out(cas_sortie_name().c_str());
      gen e(symbolic(at_debug,w));
      *logptr(contextptr) << gettext("Archiving ") << e << '\n';
      archive(child_out,e,contextptr);
      archive(child_out,zero,contextptr);
      child_out << "Debugging\n" << '¤' ;
      child_out.close();
      kill_and_wait_sigusr2();
      ifstream child_in(cas_entree_name().c_str());
      w[1]= unarchive(child_in,contextptr);
      child_in.close();
      *logptr(contextptr) << gettext("Click reads ") << w[1] << '\n';
      if (w[1].type==_SYMB){
	if (w[1]._SYMBptr->sommet==at_sst){
	  debug_ptr(contextptr)->sst_in_mode=false;
	  debug_ptr(contextptr)->sst_mode=true;
	  return;
	}
	if (w[1]._SYMBptr->sommet==at_sst_in){
	  debug_ptr(contextptr)->sst_in_mode=true;
	  debug_ptr(contextptr)->sst_mode=true;
	  return;
	}
	if (w[1]._SYMBptr->sommet==at_cont){
	  debug_ptr(contextptr)->sst_in_mode=false;
	  debug_ptr(contextptr)->sst_mode=false;
	  return;
	}
	if (w[1]._SYMBptr->sommet==at_kill){
	  _kill(0,contextptr);
	}
      }
#ifndef NO_STDEXCEPT
      try {
#endif
	w[2] =w[1].eval(1,contextptr);
#ifndef NO_STDEXCEPT
      }
      catch (std::runtime_error & error ){
	last_evaled_argptr(contextptr)=NULL;
	w[2]=string2gen(error.what(),false);
      }
#endif
    }
#endif // WIN32
  }
#endif // GIAC_HAS_STO_38
#endif // EMCC
#endif // KHICAS
  static string printasbackquote(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return "`"+feuille.print(contextptr)+"`";
  }
  gen _backquote(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return args;
  }
  static const char _backquote_s []="backquote";
  static define_unary_function_eval2 (__backquote,&_backquote,_backquote_s,&printasbackquote);
  define_unary_function_ptr( at_backquote ,alias_at_backquote ,&__backquote);

  static string printasdouble_deux_points(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    gen a,b;
    check_binary(feuille,a,b);
    string s=b.print(contextptr);
    if (b.type==_SYMB && b._SYMBptr->sommet!=at_of && b._SYMBptr->sommet.ptr()->printsommet)
      s = '('+s+')';
    if (b.type==_FUNC && s.size()>2 && s[0]=='\'' && s[s.size()-1]=='\'')
      s=s.substr(1,s.size()-2);
#ifdef GIAC_HAS_STO_38
    return a.print(contextptr)+"::"+s; // removed final space, otherwise A::B::C adds a _ in prime + " ";
#else
    return a.print(contextptr)+"::"+s+" ";
#endif
  }
  gen symb_double_deux_points(const gen & args){
    return symbolic(at_double_deux_points,args);
  }
  gen _double_deux_points(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen a,b,c;
    if (!check_binary(args,a,b))
      return a;
    if (b==at_index)
      return _struct_dot(eval(args,1,contextptr),contextptr);
    if (b==at_revlist || b==at_reverse || b==at_sort || b==at_sorted || b==at_append || b==at_prepend || b==at_concat || b==at_extend || b==at_rotate || b==at_shift || b==at_suppress || b==at_clear ){	
#if 1
	return _struct_dot(args,contextptr);
#else
	return symbolic(at_struct_dot,args);
#endif
    }
    if (b.type==_SYMB){
      unary_function_ptr c=b._SYMBptr->sommet;
      if (c==at_index)
	return _struct_dot(eval(args,1,contextptr),contextptr);
      if (c==at_revlist || c==at_reverse || c==at_sort || c==at_sorted || c==at_append || c==at_prepend || c==at_concat || c==at_extend || c==at_rotate || c==at_shift || c==at_suppress || c==at_clear ){
#if 1
	return _struct_dot(args,contextptr);
#else
	gen d=eval(a,eval_level(contextptr),contextptr);
	if (b._SYMBptr->feuille.type==_VECT && b._SYMBptr->feuille.subtype==_SEQ__VECT && b._SYMBptr->feuille._VECTptr->empty())
	  ;
	else
	  d=makesuite(d,b._SYMBptr->feuille);
	d=c(d,contextptr);
	return sto(d,a,contextptr);
#endif
      }
    }
    if (storcl_38 && abs_calc_mode(contextptr)==38 && a.type==_IDNT){
      gen value;
      if (storcl_38(value,a._IDNTptr->id_name,b.type==_IDNT?b._IDNTptr->id_name:b.print().c_str(),undef,false,contextptr,NULL,false)){
	return value;
      }
    }
#ifndef RTOS_THREADX
#if !defined BESTA_OS && !defined NSPIRE && !defined FXCG && !defined KHICAS && !defined SDL_KHICAS
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock(&context_list_mutex);
#endif
    if (a.type==_INT_ && a.subtype==0 && a.val>=0 && a.val<(int)context_list().size()){
      context * ptr = context_list()[a.val];
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&context_list_mutex);
#endif
      return eval(b,1,ptr);
    }
    if (context_names){
      map<string,context *>::iterator nt=context_names->find(a.print(contextptr)),ntend=context_names->end();
      if (nt!=ntend){
	context * ptr = nt->second;
#ifdef HAVE_LIBPTHREAD
	pthread_mutex_unlock(&context_list_mutex);
#endif
	return eval(b,1,ptr);
      }
    }
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&context_list_mutex);
#endif
#endif // RTOS
#endif
    c=b;
    if (b.is_symb_of_sommet(at_of))
      c=b._SYMBptr->feuille[0];
    string cs=c.print(contextptr);
    /* // following code not used since qualified names after export 
       // make b a symbolic not just the function name
    int l=cs.size(),j=0;
    for (;j<l-1;++j){
      if (cs[j]==':' && cs[j+1]==':')
	break;
    }
    if (j==l-1)
    */      
    cs=a.print(contextptr)+"::"+cs;
    std::pair<charptr_gen *,charptr_gen *> p= equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(cs.c_str(),0),tri);
    if (p.first!=p.second && p.first!=builtin_lexer_functions_end()){
      c=p.first->second;
      if (b.is_symb_of_sommet(at_of))
	return c(b._SYMBptr->feuille[1],contextptr);
      else
	return c;
    }
    map_charptr_gen::const_iterator it=lexer_functions().find(cs.c_str());
    if (it!=lexer_functions().end()){
      c=it->second;
      if (b.is_symb_of_sommet(at_of))
	return c(b._SYMBptr->feuille[1],contextptr);
      else
	return c;
    }
    if (b.type==_FUNC) // ? should be != _IDNT 
      return b;
    if (b.type==_SYMB)
      return b.eval(eval_level(contextptr),contextptr);
    gen aa=a.eval(1,contextptr);
    if (aa.type==_VECT)
      return find_in_folder(*aa._VECTptr,b);
    return symb_double_deux_points(args);
  }
  static const char _double_deux_points_s []="double_deux_points";
  static define_unary_function_eval2_index(91,__double_deux_points,&_double_deux_points,_double_deux_points_s,&printasdouble_deux_points);
  define_unary_function_ptr5( at_double_deux_points ,alias_at_double_deux_points,&__double_deux_points,_QUOTE_ARGUMENTS,0);

  bool is_binary(const gen & args){
    return (args.type==_VECT) && (args._VECTptr->size()==2) ;
  }

  bool check_binary(const gen & args,gen & a,gen & b){
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) ){
      a=gensizeerr(gettext("check_binary"));
      b=a;
      return false;
    }
    a=args._VECTptr->front();
    b=args._VECTptr->back();
    return true;
  }

  static bool maple2mupad(const gen & args,int in_maple_mode,int out_maple_mode,GIAC_CONTEXT){
#if defined NSPIRE || defined FXCG || defined GIAC_HAS_STO_38
    return false;
#else
    if (is_undef(check_secure()))
      return false;
    gen a,b;
    if (!check_binary(args,a,b))
      return false;
    string as,bs;
    if (a.type==_IDNT)
      as=a._IDNTptr->name();
    if (a.type==_STRNG)
      as=*a._STRNGptr;
    if (b.type==_IDNT)
      bs=b._IDNTptr->name();
    if (b.type==_STRNG)
      bs=*b._STRNGptr;
    int save_maple_mode=xcas_mode(contextptr);
    xcas_mode(contextptr)=in_maple_mode;
    ifstream infile(as.c_str());
    vecteur v;
#ifndef NO_STDEXCEPT
    try {
#endif
      readargs_from_stream(infile,v,contextptr);
#ifndef NO_STDEXCEPT
    }
    catch (std::runtime_error &  ){
      last_evaled_argptr(contextptr)=NULL;
      xcas_mode(contextptr)=save_maple_mode;
      return false;
    }
#endif
    xcas_mode(contextptr)=out_maple_mode;
    ofstream outfile(bs.c_str());
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      outfile << *it << '\n';
    xcas_mode(contextptr)=save_maple_mode;
    return true;
#endif
  }

  gen _maple2mupad(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return maple2mupad(args,1,2,contextptr);
  }
  static const char _maple2mupad_s []="maple2mupad";
  static define_unary_function_eval_quoted (__maple2mupad,&_maple2mupad,_maple2mupad_s);
  define_unary_function_ptr5( at_maple2mupad ,alias_at_maple2mupad,&__maple2mupad,_QUOTE_ARGUMENTS,true);

  gen _maple2xcas(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return maple2mupad(args,1,0,contextptr);
  }
  static const char _maple2xcas_s []="maple2xcas";
  static define_unary_function_eval_quoted (__maple2xcas,&_maple2xcas,_maple2xcas_s);
  define_unary_function_ptr5( at_maple2xcas ,alias_at_maple2xcas,&__maple2xcas,_QUOTE_ARGUMENTS,true);

  gen _mupad2maple(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return maple2mupad(args,2,1,contextptr);
  }
  static const char _mupad2maple_s []="mupad2maple";
  static define_unary_function_eval_quoted (__mupad2maple,&_mupad2maple,_mupad2maple_s);
  define_unary_function_ptr5( at_mupad2maple ,alias_at_mupad2maple,&__mupad2maple,_QUOTE_ARGUMENTS,true);

  gen _mupad2xcas(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return maple2mupad(args,2,0,contextptr);
  }
  static const char _mupad2xcas_s []="mupad2xcas";
  static define_unary_function_eval_quoted (__mupad2xcas,&_mupad2xcas,_mupad2xcas_s);
  define_unary_function_ptr5( at_mupad2xcas ,alias_at_mupad2xcas,&__mupad2xcas,_QUOTE_ARGUMENTS,true);

  // py==0 Xcas, py==1 Python-like, py==2 JS-like
  gen python_xcas(const gen & args,int py,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    // parse in current mode and print in python mode
    gen g(eval(args,1,contextptr));
    int p=python_compat(contextptr);
    if (g.type==_STRNG){
      if (py==0) 
	return string2gen(python2xcas(*g._STRNGptr,contextptr),false);
      g=gen(*g._STRNGptr,contextptr);
    }
    python_compat(py==2?0:1,contextptr);
    string s(g.print(contextptr));
    python_compat(p,contextptr);
    if (py==0)
      return string2gen(python2xcas(s,contextptr),false);
    if (py==2){
      // TODO add function, replace := by =, local by var, -> by nothing
      s=args.print()+": function"+s;
    }
    g=string2gen(s,false);
    return g;
  }

  int (*micropy_ptr) (cstcharptr)=0;
  gen _python(const gen & args,GIAC_CONTEXT){
#if defined HAVE_LIBMICROPYTHON
    if (micropy_ptr && args.type==_VECT && args._VECTptr->size()==2){
      gen a=args._VECTptr->front(),b=args._VECTptr->back();
      if (a.type==_STRNG && b==at_python){
	const char * ptr=a._STRNGptr->c_str();
	while (*ptr==' ')
	  ++ptr;
	bool gr= strcmp(ptr,"show()")==0 || strcmp(ptr,",")==0;
	bool pix =strcmp(ptr,";")==0;
	bool turt=strcmp(ptr,".")==0;
	bool xc=strcmp(ptr,"xcas")==0;
	python_contextptr=contextptr;
	python_console()="";
	gen g;
	if ( strcmp(ptr,",") && !xc && !turt && !pix ){
	  (*micropy_ptr)(ptr);
	}
	context * cascontextptr=(context *)caseval("caseval contextptr");
	if (freezeturtle || turt){
	  // copy caseval turtle to this context
	  turtle(contextptr)=turtle(cascontextptr);
	  turtle_stack(contextptr)=turtle_stack(cascontextptr);
	  return _avance(0,contextptr);
	}
	if (freeze || pix)
	  return _show_pixels(0,contextptr);
	if (gr)
	  return history_plot(cascontextptr);
	if (python_console().empty())
	  return string2gen("Done",false);
	if (python_console()[python_console().size()-1]=='\n')
	  python_console()=python_console().substr(0,python_console().size()-1);
	return string2gen(python_console().empty()?"Done":python_console(),false);
      }
    }
#endif
#if defined MICROPY_LIB
    if (micropy_ptr && args.type==_VECT && args._VECTptr->size()==2){
      gen a=args._VECTptr->front(),b=args._VECTptr->back();
      if (a.type==_STRNG && b==at_python){
	const char * ptr=a._STRNGptr->c_str();
	(*micropy_ptr)(ptr);
	return string2gen("Done",false);
      }
    }
#endif
    return python_xcas(args,1,contextptr);
  }
  static const char _python_s []="python";
  static define_unary_function_eval_quoted (__python,&_python,_python_s);
  define_unary_function_ptr5( at_python ,alias_at_python,&__python,_QUOTE_ARGUMENTS,true);

  gen _xcas(const gen & args,GIAC_CONTEXT){
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==0){
      giac::system_browser_command("xcas.html");
      return 1;
    }
    return python_xcas(args,0,contextptr);
  }
  static const char _xcas_s []="xcas";
  static define_unary_function_eval_quoted (__xcas,&_xcas,_xcas_s);
  define_unary_function_ptr5( at_xcas ,alias_at_xcas,&__xcas,_QUOTE_ARGUMENTS,true);

  char * (*quickjs_ptr) (cstcharptr)=0;
  gen _javascript(const gen & args,GIAC_CONTEXT){
#if defined QUICKJS
    if (quickjs_ptr && args.type==_VECT && args._VECTptr->size()==2){
      gen a=args._VECTptr->front(),b=args._VECTptr->back();
      if (a.type==_STRNG && b==at_javascript){
	const char * ptr=a._STRNGptr->c_str();
	while (*ptr==' ')
	  ++ptr;
	bool gr= strcmp(ptr,"show()")==0 || strcmp(ptr,",")==0;
	bool pix =strcmp(ptr,";")==0;
	bool turt=strcmp(ptr,".")==0;
	bool xc=strcmp(ptr,"xcas")==0;
	python_contextptr=contextptr;
	gen g; string ans;
	if (!gr && !xc && !turt && !pix ){
	  char * ansptr=(*quickjs_ptr)(ptr);
	  if (ansptr){
	    ans=ansptr;
	    free(ansptr);
	  }
	}
	context * cascontextptr=(context *)caseval("caseval contextptr");
	if (freeze || pix)
	  return _show_pixels(0,cascontextptr);
	if (gr || ans=="Graphic_object" )
	  return history_plot(cascontextptr);
	if (ans.empty())
	  return string2gen("Done",false);
	if (ans[ans.size()-1]=='\n')
	  ans=ans.substr(0,ans.size()-1);
	g=gen(ans,contextptr);
	if (first_error_line(contextptr)>0) g=string2gen(ans.empty()?"Done":ans,false);
#if !defined KHICAS && !defined SDL_KHICAS
	if (freezeturtle || turt || islogo(g) || ans=="Logo_turtle"){
	  // copy caseval turtle to this context
	  turtle(contextptr)=turtle(cascontextptr);
	  turtle_stack(contextptr)=turtle_stack(cascontextptr);
	  return _avance(0,contextptr);
	}
	CERR << "";
#endif
	return g;
      }
    }
#endif
    return python_xcas(args,2,contextptr);
  }
  static const char _javascript_s []="javascript";
  static define_unary_function_eval_quoted (__javascript,&_javascript,_javascript_s);
  define_unary_function_ptr5( at_javascript ,alias_at_javascript,&__javascript,_QUOTE_ARGUMENTS,true);

  static string printasvirgule(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    return feuille._VECTptr->front().print(contextptr)+','+feuille._VECTptr->back().print(contextptr);
  }
  gen _virgule(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return args;
    const_iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (itend-it<2)
      return args;
    gen res=makesuite(*it,*(it+1));
    ++it;
    ++it;
    for (;it!=itend;++it)
      res=makesuite(res,*it);
    return res;
  }
  static const char _virgule_s []="virgule";
  static define_unary_function_eval2 (__virgule,&_virgule,_virgule_s,&printasvirgule);
  define_unary_function_ptr( at_virgule ,alias_at_virgule ,&__virgule);

  gen _pwd(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
#ifndef HAVE_NO_CWD
    char * buffer=getcwd(0,0);
    if (buffer){
      string s(buffer);
#ifndef HAVE_LIBGC
      free(buffer);
#endif
      return string2gen(s,false);
    }
#endif
    return gensizeerr(contextptr);
  }
  static const char _pwd_s []="pwd";
  static define_unary_function_eval (__pwd,&_pwd,_pwd_s);
  define_unary_function_ptr5( at_pwd ,alias_at_pwd,&__pwd,0,true);

  gen _cd(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen tmp=check_secure();
    if (is_undef(tmp)) return tmp;
    if (args.type!=_STRNG)
      return gentypeerr(contextptr);
    int res;
    string s(*args._STRNGptr);
    string ss(*_pwd(zero,contextptr)._STRNGptr+'/'),current;
    int l=int(s.size());
    for (int i=0;i<=l;i++){
      if ( (i==l) || (s[i]=='/') ){
	if (i){
	  if (current==".."){
	    int t=int(ss.size())-2;
	    for (;t>0;--t){
	      if (ss[t]=='/')
		break;
	    }
	    if (t)
	      ss=ss.substr(0,t+1);
	    else
	      ss="/";
	  } 
	  else { // not ..
	    if (current[0]=='~'){
	      if (current.size()==1){ // uid user directory
		ss = home_directory();
	      }
	      else { // other user directory
		current=current.substr(1,current.size()-1);
#if !defined HAVE_NO_PWD_H && !defined NSPIRE_NEWLIB && !defined KHICAS && !defined SDL_KHICAS
		passwd * p=getpwnam(current.c_str());
		if (!p)
		  return gensizeerr(gettext("No such user ")+current);
		ss = p->pw_dir ;
		ss +='/';
#else
		ss = "/";
#endif
	      }
	    }
	    else
	      ss+=current+"/";
	  } // end .. detection
	}
	else // i==0 / means absolute path
	  ss="/";
	current="";
      } // end / detection
      else {
	if (s[i]>' ')
	  current += s[i];
      }
    } // end for
#ifndef HAVE_NO_CWD
    res=chdir(ss.c_str());
#else
    res=-1;
#endif
    if (res)
      return gensizeerr(contextptr);
    gen g=symbolic(at_cd,_pwd(zero,contextptr));
#ifdef HAVE_SIGNAL_H_OLD
    if (!child_id)
      _signal(symb_quote(g),contextptr);
#endif
    // *logptr(contextptr) << g << '\n';
    return g;
  }
  static const char _cd_s []="cd";
  static define_unary_function_eval (__cd,&_cd,_cd_s);
  define_unary_function_ptr5( at_cd ,alias_at_cd,&__cd,0,true);

#if !defined GIAC_GGB && !defined NUMWORKS && !defined EMCC2
  // unix command sha256sum
  gen _sha256(const gen &g_,GIAC_CONTEXT){
    int offset=0;
    gen g(g_);
    if (g.type==_VECT && g._VECTptr->size()==2 && g._VECTptr->back().type==_INT_){
      g=g_._VECTptr->front();
      offset=g_._VECTptr->back().val;
    }
    if (offset<0 || g.type!=_STRNG || g._STRNGptr->size()>240)
      return gensizeerr(contextptr);
    int bufsize=64*1024;
    unsigned char * buf=(unsigned char *) malloc(bufsize);
    if (!buf){
      *logptr(contextptr)<< "Memory error\n";
      return -1;
    }
    char filename[256]={0};
#ifdef FXCG
    strcat(filename,"\\\\fls0\\");
    strcat(filename,g._STRNGptr->c_str());
    unsigned short pFile[256];
    Bfile_StrToName_ncpy(pFile, (const unsigned char *)filename, strlen(filename)+1);
    int hFile = Bfile_OpenFile_OS(pFile, READWRITE); // Get handle
    if (hFile<0){
      *logptr(contextptr) << "File not found\n";
      return -2;
    }
    int size = Bfile_GetFileSize_OS(hFile);
    if (offset>size){
      Bfile_CloseFile_OS(hFile);
      free(buf);
      return -4;
    }
    if (offset){ // skip
      Bfile_ReadFile_OS(hFile, buf,offset, -1);
      size -= offset;
    }
    SHA256_CTX ctx;
    sha256_init(&ctx);
    while (size){
      int rsize=0,Size=size>bufsize?bufsize:size;
      rsize = Bfile_ReadFile_OS(hFile, buf,Size, -1);
      if (rsize!=Size){
        Bfile_CloseFile_OS(hFile);
        free(buf);
        return -3;
      }
      sha256_update(&ctx,buf,Size);
      size -= Size;
    }
    Bfile_CloseFile_OS(hFile);
    free(buf);
    std::vector<unsigned char> v(SHA256_BLOCK_SIZE);
    BYTE * hash=&v.front();
    sha256_final(&ctx,hash);
    vecteur V;
    for (int i=0;i<v.size();++i)
      V.push_back(v[i]);
    return V;
#else
    strcat(filename,g._STRNGptr->c_str());
    FILE * hFile = fopen(filename,"rb");
    if (!hFile){
      *logptr(contextptr) << "File not found\n";
      return -2;
    }
    fseek(hFile, 0, SEEK_END);
    int size = ftell(hFile);
    fseek(hFile,0,SEEK_SET);
    if (offset>size){
      fclose(hFile);
      free(buf);
      return -4;
    }
    if (offset){ // skip
      fread(buf,1,offset,hFile);
      size -=offset;
    }
    SHA256_CTX ctx;
    sha256_init(&ctx);
    while (size){
      int rsize=0,Size=size>bufsize?bufsize:size;
      rsize = fread(buf,1,Size,hFile); 
      if (rsize!=Size){
        fclose(hFile);
        free(buf);
        return -3;
      }
      sha256_update(&ctx,buf,Size);
      size -= Size;
    }
    fclose(hFile);
    free(buf);
    std::vector<unsigned char> v(SHA256_BLOCK_SIZE);
    BYTE * hash=&v.front();
    sha256_final(&ctx,hash);
    vecteur V;
    for (int i=0;i<v.size();++i)
      V.push_back(v[i]);
    return V;
#endif
  }
  static const char _sha256_s []="sha256";
  static define_unary_function_eval (__sha256,&_sha256,_sha256_s);
  define_unary_function_ptr5( at_sha256 ,alias_at_sha256,&__sha256,0,true);
#endif
  
  gen _scientific_format(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
#if 0 // ndef KHICAS
    gen tmp=check_secure();
    if (is_undef(tmp)) return tmp;
#endif
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return scientific_format(contextptr);
    scientific_format(args.val,contextptr);
    return args;
  }
  static const char _scientific_format_s []="scientific_format";
  static define_unary_function_eval2 (__scientific_format,&_scientific_format,_scientific_format_s,&printasDigits);
  define_unary_function_ptr( at_scientific_format ,alias_at_scientific_format ,&__scientific_format);

  gen _integer_format(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
#if 0 // ndef KHICAS
    gen tmp=check_secure(); if (is_undef(tmp)) return tmp;
#endif
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return integer_format(contextptr);
    integer_format(args.val,contextptr);
    return args;
  }
  static const char _integer_format_s []="integer_format";
  static define_unary_function_eval2 (__integer_format,&_integer_format,_integer_format_s,&printasDigits);
  define_unary_function_ptr5( at_integer_format ,alias_at_integer_format,&__integer_format,0,true);

  // 0: xcas, 1: maple, 2: mupad, 3: ti, 256:xcas/python
  gen _xcas_mode(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return xcas_mode(contextptr);
    xcas_mode(contextptr)=args.val & 0xff;
    python_compat(args.val/256,contextptr);
    return string2gen(gettext("Warning: some commands like subs might change arguments order"),false);
  }
  static const char _xcas_mode_s []="xcas_mode";
  static define_unary_function_eval (__xcas_mode,&_xcas_mode,_xcas_mode_s);
  define_unary_function_ptr5( at_xcas_mode ,alias_at_xcas_mode,&__xcas_mode,0,true);
  static const char _maple_mode_s []="maple_mode";
  static define_unary_function_eval (__maple_mode,&_xcas_mode,_maple_mode_s);
  define_unary_function_ptr5( at_maple_mode ,alias_at_maple_mode,&__maple_mode,0,true);
  gen _python_compat(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    int p=python_compat(contextptr);
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    if (args.type==_VECT && args._VECTptr->size()==3){
      vecteur & v=*args._VECTptr;
      gen a=v[0],b=v[1],c=v[2];
      if (is_integral(a) && is_integral(b) && is_integral(c)){
	python_compat(a.val,contextptr) ;
#ifdef KHICAS
	pythonjs_heap_size=giacmax(absint(b.val),16*1024);
	pythonjs_stack_size=giacmax(absint(c.val),8*1024);
#endif
	return p;
      }
    }
    if (args.type==_VECT && args._VECTptr->empty())
      return p;
    if (args.type!=_INT_)
      return gensizeerr(contextptr);
    python_compat(args.val,contextptr) ;
    return p;
  }
  static const char _python_compat_s []="python_compat";
  static define_unary_function_eval (__python_compat,&_python_compat,_python_compat_s);
  define_unary_function_ptr5( at_python_compat ,alias_at_python_compat,&__python_compat,0,true);

  gen giac_eval_level(const gen & g,GIAC_CONTEXT){
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return eval_level(contextptr);
    eval_level(contextptr)=giacmax(args.val,1);
    DEFAULT_EVAL_LEVEL=args.val;
    return args;
  }
  static const char _eval_level_s []="eval_level";
  static define_unary_function_eval2 (__eval_level,&giac_eval_level,_eval_level_s,&printasDigits);
  define_unary_function_ptr5( at_eval_level ,alias_at_eval_level,&__eval_level,0,true);

  gen _prog_eval_level(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return prog_eval_level(contextptr);
    prog_eval_level_val(contextptr)=args.val;
    return args;
  }
  static const char _prog_eval_level_s []="prog_eval_level";
  static define_unary_function_eval2 (__prog_eval_level,&_prog_eval_level,_prog_eval_level_s,&printasDigits);
  define_unary_function_ptr5( at_prog_eval_level ,alias_at_prog_eval_level,&__prog_eval_level,0,true);

  gen _with_sqrt(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return withsqrt(contextptr);
    withsqrt(contextptr)=(args.val)!=0;
    return args;
  }
  static const char _with_sqrt_s []="with_sqrt";
  static define_unary_function_eval2 (__with_sqrt,&_with_sqrt,_with_sqrt_s,&printasDigits);
  define_unary_function_ptr5( at_with_sqrt ,alias_at_with_sqrt,&__with_sqrt,0,true);

  gen _all_trig_solutions(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return all_trig_sol(contextptr);
    all_trig_sol((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _all_trig_solutions_s []="all_trig_solutions";
  static define_unary_function_eval2 (__all_trig_solutions,&_all_trig_solutions,_all_trig_solutions_s,&printasDigits);
  define_unary_function_ptr( at_all_trig_solutions ,alias_at_all_trig_solutions ,&__all_trig_solutions);

  gen _increasing_power(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return increasing_power(contextptr);
    increasing_power((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _increasing_power_s []="increasing_power";
  static define_unary_function_eval2 (__increasing_power,&_increasing_power,_increasing_power_s,&printasDigits);
  define_unary_function_ptr( at_increasing_power ,alias_at_increasing_power ,&__increasing_power);

  gen _ntl_on(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return ntl_on(contextptr);
    ntl_on((args.val)!=0,contextptr);
    ntl_on((args.val)!=0,context0); // Current factorization routines do not have access to the context
    return args;
  }
  static const char _ntl_on_s []="ntl_on";
  static define_unary_function_eval2 (__ntl_on,&_ntl_on,_ntl_on_s,&printasDigits);
  define_unary_function_ptr( at_ntl_on ,alias_at_ntl_on ,&__ntl_on);

#ifdef GIAC_HAS_STO_38
  gen aspen_HComplex(int i);
#endif  
  gen _complex_mode(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return complex_mode(contextptr);
    complex_mode((args.val)!=0,contextptr);
#ifdef GIAC_HAS_STO_38
    aspen_HComplex(args.val);
#endif      
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _complex_mode_s []="complex_mode";
  static define_unary_function_eval2 (__complex_mode,&_complex_mode,_complex_mode_s,&printasDigits);
  define_unary_function_ptr( at_complex_mode ,alias_at_complex_mode ,&__complex_mode);

  gen _keep_algext(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return keep_algext(contextptr);
    keep_algext((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _keep_algext_s []="keep_algext";
  static define_unary_function_eval2 (__keep_algext,&_keep_algext,_keep_algext_s,&printasDigits);
  define_unary_function_ptr( at_keep_algext ,alias_at_keep_algext ,&__keep_algext);

  gen _auto_assume(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    if (g.type==_SYMB || g.type==_IDNT)
      return autoassume(g,vx_var,contextptr);
    if (args.type!=_INT_)
      return auto_assume(contextptr);
    auto_assume((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _auto_assume_s []="auto_assume";
  static define_unary_function_eval (__auto_assume,&_auto_assume,_auto_assume_s);
  define_unary_function_ptr5( at_auto_assume ,alias_at_auto_assume ,&__auto_assume,0,true);

  gen _parse_e(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    if (args.type!=_INT_)
      return parse_e(contextptr);
    parse_e((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _parse_e_s []="parse_e";
  static define_unary_function_eval (__parse_e,&_parse_e,_parse_e_s);
  define_unary_function_ptr5( at_parse_e ,alias_at_parse_e ,&__parse_e,0,true);

  gen _convert_rootof(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    if (args.type!=_INT_)
      return convert_rootof(contextptr);
    convert_rootof((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _convert_rootof_s []="convert_rootof";
  static define_unary_function_eval (__convert_rootof,&_convert_rootof,_convert_rootof_s);
  define_unary_function_ptr5( at_convert_rootof ,alias_at_convert_rootof ,&__convert_rootof,0,true);

#ifdef GIAC_HAS_STO_38
  gen aspen_HAngle(int i);
#endif
  gen _angle_radian(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    //grad
    //since this is a programming command, not sure what you want done here exactly..., will return 1 for radian, 0 for degree, and 2 for grads to match prior behavior
    if (args.type!=_INT_){
      if(angle_radian(contextptr))
        return 1;
      else if(angle_degree(contextptr))
        return 0;
      else
        return 2;
    }
    //anything but 0 or 2 will result in radians...
    int val = args.val;
    if(val == 0) 
      angle_mode(1, contextptr); //set to degrees if 0
    else if(val==2)
      angle_mode(2, contextptr); //set to grads if 2
    else
      angle_mode(0, contextptr); //set to radians for anything but those two
#ifdef GIAC_HAS_STO_38
    aspen_HAngle(val==0?1:0);
#endif
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _angle_radian_s []="angle_radian";
  static define_unary_function_eval2 (__angle_radian,&_angle_radian,_angle_radian_s,&printasDigits);
  define_unary_function_ptr( at_angle_radian ,alias_at_angle_radian ,&__angle_radian);

  gen _epsilon(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG &&  arg.subtype==-1) return  arg;
    gen args=evalf_double(arg,0,contextptr);
    if (args.type!=_DOUBLE_)
      return epsilon(contextptr);
    epsilon(fabs(args._DOUBLE_val),contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _epsilon_s []="epsilon";
  static define_unary_function_eval2 (__epsilon,&_epsilon,_epsilon_s,&printasDigits);
  define_unary_function_ptr( at_epsilon ,alias_at_epsilon ,&__epsilon);

  gen _proba_epsilon(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG &&  arg.subtype==-1) return  arg;
    gen args=evalf_double(arg,0,contextptr);
    if (args.type!=_DOUBLE_)
      return proba_epsilon(contextptr);
    proba_epsilon(contextptr)=fabs(args._DOUBLE_val);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _proba_epsilon_s []="proba_epsilon";
  static define_unary_function_eval2 (__proba_epsilon,&_proba_epsilon,_proba_epsilon_s,&printasDigits);
  define_unary_function_ptr( at_proba_epsilon ,alias_at_proba_epsilon ,&__proba_epsilon);

  gen _complex_variables(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return complex_variables(contextptr);
    complex_variables((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _complex_variables_s []="complex_variables";
  static define_unary_function_eval2 (__complex_variables,&_complex_variables,_complex_variables_s,&printasDigits);
  define_unary_function_ptr( at_complex_variables ,alias_at_complex_variables ,&__complex_variables);

  gen _approx_mode(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);
    if (args.type!=_INT_)
      return approx_mode(contextptr);
    approx_mode((args.val)!=0,contextptr);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _approx_mode_s []="approx_mode";
  static define_unary_function_eval2 (__approx_mode,&_approx_mode,_approx_mode_s,&printasDigits);
  define_unary_function_ptr( at_approx_mode ,alias_at_approx_mode ,&__approx_mode);

  gen _threads(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return threads;
    threads=giacmax(absint(args.val),1);
    parent_cas_setup(contextptr);
    return args;
  }
  static const char _threads_s []="threads";
  static define_unary_function_eval2 (__threads,&_threads,_threads_s,&printasDigits);
  define_unary_function_ptr( at_threads ,alias_at_threads ,&__threads);

  int digits2bits(int n){
#ifdef OLDGNUWINCE
    return (n*33)/10;
#else
    return int(std::floor(std::log(10.0)/std::log(2.0)*n))+1;
#endif
  }

  int bits2digits(int n){
#ifdef OLDGNUWINCE
    return (n*3)/10;
#else
    return int(std::floor(std::log(2.0)/std::log(10.0)*n))+1;
#endif
  }

  void set_decimal_digits(int n,GIAC_CONTEXT){
#ifdef GNUWINCE
    return ;
#else
#ifdef HAVE_LIBMPFR
    decimal_digits(contextptr)=giacmax(absint(n),1);
#else
    decimal_digits(contextptr)=giacmin(giacmax(absint(n),1),13);
#endif
    // deg2rad_g=evalf(cst_pi,1,0)/180;
    // rad2deg_g=inv(deg2rad_g);
#endif
  }

  bool cas_setup(const vecteur & v_orig,GIAC_CONTEXT){
    vecteur v(v_orig);
    for (size_t i=0;i<v.size();++i){
      gen g=v[i];
      if (g.type==_VECT && g._VECTptr->size()==2 && g._VECTptr->front().type==_STRNG)
	v[i]=g._VECTptr->back();
    }
    if (v.size()<7)
      return false;
    if (logptr(contextptr) && debug_infolevel) 
      *logptr(contextptr) << gettext("Cas_setup ") << v << char(10) << char(13) ;
    if (v[0].type==_INT_)
      approx_mode((v[0].val)!=0,contextptr);
    else {
      v[0]=evalf_double(v[0],1,contextptr);
      if (v[0].type==_DOUBLE_)
	approx_mode(v[0]._DOUBLE_val!=0,contextptr);
    }
    if (v[1].type==_INT_)
      complex_variables((v[1].val)!=0,contextptr);
    else {
      v[1]=evalf_double(v[1],1,contextptr);
      if (v[1].type==_DOUBLE_)
	complex_variables(v[1]._DOUBLE_val!=0,contextptr);
    }
    if (v[2].type==_INT_)
      complex_mode((v[2].val)!=0,contextptr);
    else {
      v[2]=evalf_double(v[2],1,contextptr);
      if (v[2].type==_DOUBLE_)
	complex_mode(v[2]._DOUBLE_val!=0,contextptr);
    }
    if (v[3].type==_INT_)
    {
      //grad
      //since end user sees val !=0 being radians, I have hijacked so 2 will be grad, 0 is deg, and anything else is radians
      int val = v[3].val;
      if(val == 0)
        angle_mode(1, contextptr); //degrees if ==0
      else if(val == 2)
        angle_mode(2, contextptr); //grad if ==2
      else
        angle_mode(0, contextptr); //anything else is radians
    }
    else {
      v[3]=evalf_double(v[3],1,contextptr);
      if (v[3].type==_DOUBLE_)
      {
        //grad
        //since end user sees val !=0 being radians, I have hijacked so 2 will be grad, 0 is deg, and anything else is radians
        int val = int(v[3]._DOUBLE_val);
        if(val == 0)
          angle_mode(1, contextptr); //degrees if ==0
        else if(val == 2)
          angle_mode(2, contextptr); //grad if ==2
        else
          angle_mode(0, contextptr); //anything else is radians
      }
    }
    v[4]=evalf_double(v[4],1,contextptr);
    if (v[4].type==_DOUBLE_){
      int format=int(v[4]._DOUBLE_val);
      scientific_format(format % 16,contextptr);
      integer_format(format/16,contextptr);
    }
    v[5]=evalf_double(v[5],1,contextptr);
    if (v[5].type==_DOUBLE_)
      epsilon(fabs(v[5]._DOUBLE_val),contextptr);
    if (v[5].type==_VECT && v[5]._VECTptr->size()==2 && v[5]._VECTptr->front().type==_DOUBLE_ && v[5]._VECTptr->back().type==_DOUBLE_){
      epsilon(fabs(v[5]._VECTptr->front()._DOUBLE_val),contextptr);
      proba_epsilon(contextptr)=fabs(v[5]._VECTptr->back()._DOUBLE_val); 
    }
    if (v[6].type==_INT_)
      set_decimal_digits(v[6].val,contextptr);
    else {
      v[6]=evalf_double(v[6],1,contextptr);
      if (v[6].type==_DOUBLE_)
	set_decimal_digits(int(v[6]._DOUBLE_val),contextptr);
    }
    if (v.size()>=8){
      if (v[7].type==_VECT){
	vecteur & vv =*v[7]._VECTptr;
	if (vv.size()>=4){
	  threads=std::max(1,int(evalf_double(vv[0],1,contextptr)._DOUBLE_val));
	  MAX_RECURSION_LEVEL=std::max(int(evalf_double(vv[1],1,contextptr)._DOUBLE_val),1);
	  debug_infolevel=std::max(0,int(evalf_double(vv[2],1,contextptr)._DOUBLE_val));
	  DEFAULT_EVAL_LEVEL=std::max(1,int(evalf_double(vv[3],1,contextptr)._DOUBLE_val));
	}
      }
    }
    if (v.size()>=9){ 
      if (v[8].type==_INT_)
	increasing_power(v[8].val!=0,contextptr);
      else {
	v[8]=evalf_double(v[8],1,contextptr);
	if (v[8].type==_DOUBLE_)
	  increasing_power(v[8]._DOUBLE_val!=0,contextptr);
      }
    }
    if (v.size()>=10){ 
      if (v[9].type==_INT_)
	withsqrt(v[9].val!=0,contextptr);
      else {
	v[9]=evalf_double(v[9],1,contextptr);
	if (v[9].type==_DOUBLE_)
	  withsqrt(v[9]._DOUBLE_val!=0,contextptr);
      }
    }
    if (v.size()>=11){ 
      if (v[10].type==_INT_)
	all_trig_sol(v[10].val!=0,contextptr);
      else {
	v[10]=evalf_double(v[10],1,contextptr);
	if (v[10].type==_DOUBLE_)
	  all_trig_sol(v[10]._DOUBLE_val!=0,contextptr);
      }
    }
    if (v.size()>=12){ 
      if (v[11].type==_INT_)
	integer_mode(v[11].val!=0,contextptr);
      else {
	v[11]=evalf_double(v[11],1,contextptr);
	if (v[11].type==_DOUBLE_)
	  integer_mode(v[11]._DOUBLE_val!=0,contextptr);
      }
    }
    return true;
  }
  vecteur cas_setup(GIAC_CONTEXT){
    vecteur v;
    v.push_back(approx_mode(contextptr));
    v.push_back(complex_variables(contextptr));
    v.push_back(complex_mode(contextptr));
    int an=angle_mode(contextptr);
    v.push_back(an==2?2:1-an); //grad //not sure if this will mess anything up on your side bernard, having int instead of bool
    v.push_back(scientific_format(contextptr)+16*integer_format(contextptr));
    v.push_back(makevecteur(epsilon(contextptr),proba_epsilon(contextptr)));
    v.push_back(decimal_digits(contextptr));
    v.push_back(makevecteur(threads,MAX_RECURSION_LEVEL,debug_infolevel,DEFAULT_EVAL_LEVEL));
    v.push_back(increasing_power(contextptr));
    v.push_back(withsqrt(contextptr));
    v.push_back(all_trig_sol(contextptr));
    v.push_back(integer_mode(contextptr));
    return v;
  }
  gen _cas_setup(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT){
      vecteur v=cas_setup(contextptr);
      v[0]=makevecteur(string2gen("~",false),v[0]);
      v[1]=makevecteur(string2gen("var C",false),v[1]);
      v[2]=makevecteur(string2gen("C",false),v[2]);
      v[3]=makevecteur(string2gen("0:deg/1:rad/2:grad",false),v[3]);
      v[4]=makevecteur(string2gen("format",false),v[4]);
      v[5]=makevecteur(string2gen("[epsilon,proba_epsilon]",false),v[5]);
      v[6]=makevecteur(string2gen("digits",false),v[6]);
      v[7]=makevecteur(string2gen("[thread,recursion,debug,eval]",false),v[7]);
      v[8]=makevecteur(string2gen("increasing power",false),v[8]);      
      v[9]=makevecteur(string2gen("sqrt",false),v[9]);
      v[10]=makevecteur(string2gen("trig. solutions",false),v[10]);
      v[11]=makevecteur(string2gen("integer",false),v[11]);
      return v;
    }
    vecteur & w=*args._VECTptr;
    if (w.empty())
      return cas_setup(contextptr);
    if (!cas_setup(w,contextptr))
      return gendimerr(contextptr);
#ifdef HAVE_SIGNAL_H_OLD
    if (!child_id){
      _signal(symbolic(at_quote,symbolic(at_cas_setup,w)),contextptr);
    }
#endif
    return args;
  }
  static const char _cas_setup_s []="cas_setup";
  static define_unary_function_eval (__cas_setup,&_cas_setup,_cas_setup_s);
  define_unary_function_ptr5( at_cas_setup ,alias_at_cas_setup,&__cas_setup,0,true);

  void parent_cas_setup(GIAC_CONTEXT){
#ifdef HAVE_SIGNAL_H_OLD
    if (!child_id){
      _signal(symbolic(at_quote,symbolic(at_cas_setup,cas_setup(contextptr))),contextptr);
    }
#endif
  }

  string printasDigits(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type==_VECT && feuille._VECTptr->empty())
      return sommetstr;
    return sommetstr+(" := "+feuille.print(contextptr));
  }
  gen _Digits(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    if (g.type==_DOUBLE_)
      args=int(g._DOUBLE_val);    
    if (args.type!=_INT_)
      return decimal_digits(contextptr);
    set_decimal_digits(args.val,contextptr);
    bf_global_prec=std::ceil(3.3219280948873623
			     //M_LN10/M_LN2
			     *giacmax(absint(args.val),1));
    _cas_setup(cas_setup(contextptr),contextptr);
    return decimal_digits(contextptr);
  }
  static const char _Digits_s []="Digits";
  static define_unary_function_eval2 (__Digits,&_Digits,_Digits_s,&printasDigits);
  define_unary_function_ptr( at_Digits ,alias_at_Digits ,&__Digits);

  gen _xport(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    string libname(gen2string(args));
    if (libname.size()>5 && libname.substr(0,5)=="giac_")
      libname=libname.substr(5,libname.size()-5);
#ifdef USTL
    ustl::map<std::string,std::vector<std::string> >::iterator it=library_functions().find(libname);
    if (it==library_functions().end())
      return zero;
    ustl::sort(it->second.begin(),it->second.end());
#else
    std::map<std::string,std::vector<std::string> >::iterator it=library_functions().find(libname);
    if (it==library_functions().end())
      return zero;
    sort(it->second.begin(),it->second.end());
#endif
    // Add library function names to the translator
    std::vector<std::string>::iterator jt=it->second.begin(),jtend=it->second.end(),kt,ktend;
    for (;jt!=jtend;++jt){
      string tname=libname+"::"+*jt;
      // Find if the name exists in the translator base
      it=lexer_translator().find(*jt);
      if (it==lexer_translator().end())
	lexer_translator()[*jt]=vector<string>(1,tname);
      else { // Name exists, check if tname is in the vector, else push it
	kt=it->second.begin(); ktend=it->second.end();
	for (;kt!=ktend;++kt){
	  if (*kt==tname)
	    break;
	}
	if (kt!=ktend)
	  it->second.erase(kt);
	it->second.push_back(tname);
      }
    }
    return plus_one;
  }
  gen _insmod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_IDNT)
      return _insmod(string2gen(args.print(contextptr),false),contextptr);
    if (args.type!=_STRNG)
      return _xport(args,contextptr);
#ifdef __APPLE__
    string suffix=".dylib";
#else
    string suffix=".so";
#endif
    size_t sl=suffix.size();
#ifdef HAVE_LIBDL
    string libname=*args._STRNGptr;
    if (libname.empty())
      return 0;
    // a way to add the current path to the search
    if (libname[0]!='/'){
      if (libname.size()<3 || libname.substr(0,3)!="lib")
	libname = "libgiac_"+libname;
      gen pwd=_pwd(0,contextptr);
      if (pwd.type==_STRNG){
	string libname1 = *pwd._STRNGptr+'/'+libname;
	if (libname1.size()<sl || libname1.substr(libname1.size()-sl,sl)!=suffix)
	  libname1 += suffix;
	if (is_file_available(libname1.c_str()))
	  libname=libname1;
      }
    }
#ifndef WIN32
    if (libname.size()<sl || libname.substr(libname.size()-sl,sl)!=suffix)
      libname += suffix;
#endif
    modules_tab::const_iterator i = giac_modules_tab.find(libname);
    if (i!=giac_modules_tab.end())
      return 3; // still registered
    registered_lexer_functions().clear();
    doing_insmod=true;
    void * handle = dlopen (libname.c_str(), RTLD_LAZY);
    if (!handle) {
      setsizeerr (string(dlerror()));
    }
    // if (debug_infolevel)
    //  *logptr(contextptr) << registered_lexer_functions << '\n';
    giac_modules_tab[libname]=module_info(registered_lexer_functions(),handle);
#ifdef HAVE_SIGNAL_H_OLD
    if (!child_id)
      _signal(symb_quote(symbolic(at_insmod,args)),contextptr);
    else
#endif
      if (debug_infolevel) *logptr(contextptr) << gettext("Parent insmod") <<'\n';
    return 1+_xport(args,contextptr);
#else // HAVE_LIBDL
    return zero;
#endif // HAVE_LIBDL
  }
  static const char _insmod_s []="insmod";
  static define_unary_function_eval (__insmod,&_insmod,_insmod_s);
  define_unary_function_ptr5( at_insmod ,alias_at_insmod,&__insmod,0,true);
  // QUOTE_ARGUMENTS ??

  gen _rmmod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_STRNG)
      return gentypeerr(contextptr);
#ifdef HAVE_LIBDL
    string libname=*args._STRNGptr;
    modules_tab::const_iterator i = giac_modules_tab.find(libname);
    if (i==giac_modules_tab.end())
      return plus_two; // not registered
    dlclose(i->second.handle);
    bool res= lexer_function_remove(i->second.registered_names);
    giac_modules_tab.erase(libname);
#ifdef HAVE_SIGNAL_H_OLD
    if (!child_id)
      _signal(symb_quote(symbolic(at_rmmod,args)),contextptr);
#endif
    return(res);
#else // HAVE_LIBDL
    return zero;
#endif // HAVE_LIBDL
  }

  /*
  gen _rmmod(const gen & args){
  if ( args){
    if (args.type==_VECT)
      apply(args.type==_STRNG &&  args.subtype==-1{
    if (args.type==_VECT)
      apply(args)) return  args){
    if (args.type==_VECT)
      apply(args;
    if (args.type==_VECT)
      apply(args,rmmod);
    rmmod(args);    
  }
  */
  static const char _rmmod_s []="rmmod";
  static define_unary_function_eval (__rmmod,&_rmmod,_rmmod_s);
  define_unary_function_ptr5( at_rmmod ,alias_at_rmmod,&__rmmod,0,true);

  gen _lsmod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    vecteur v;
#ifdef HAVE_LIBDL
    modules_tab::const_iterator i = giac_modules_tab.begin(),iend=giac_modules_tab.end();
    for (;i!=iend;++i)
      v.push_back(string2gen(i->first,false));
#endif
    return v;
  }
  static const char _lsmod_s []="lsmod";
  static define_unary_function_eval (__lsmod,&_lsmod,_lsmod_s);
  define_unary_function_ptr5( at_lsmod ,alias_at_lsmod,&__lsmod,0,true);

  class gen_sort {
    gen sorting_function;
    const context * contextptr;
  public:
    bool operator () (const gen & a,const gen & b){
      gen c=sorting_function(gen(makevecteur(a,b),_SEQ__VECT),contextptr);
      if (c.type!=_INT_){
#ifndef NO_STDEXCEPT
	setsizeerr(gettext("Unable to sort ")+c.print(contextptr));
#else
	*logptr(contextptr) << gettext("Unable to sort ") << c << '\n';
#endif
	return true;
      }
      return !is_zero(c);
    }
    gen_sort(const gen & f,const context * ptr): sorting_function(f),contextptr(ptr) {};
    gen_sort(): sorting_function(at_inferieur_strict),contextptr(0) {};
  };

  /*
  gen sorting_function;
  bool sort_sort(const gen & a,const gen & b){
    gen c=sorting_function(gen(makevecteur(a,b),_SEQ__VECT),0);
    if (c.type!=_INT_)
      setsizeerr(gettext("Unable to sort ")+c.print(contextptr));
    return !is_zero(c);
  }
  */

  /*
  gen negdistrib(const gen & g,GIAC_CONTEXT){
    if (!g.is_symb_of_sommet(at_plus))
      return -g;
    return _plus(-g._SYMBptr->feuille,contextptr);
  }
  */

  gen simplifier(const gen & g,GIAC_CONTEXT){
#if 0 // def NSPIRE
    return g;
#endif
    if (g.type!=_SYMB || g._SYMBptr->sommet==at_program || g._SYMBptr->sommet==at_pnt || g._SYMBptr->sommet==at_animation || g._SYMBptr->sommet==at_unit || g._SYMBptr->sommet==at_integrate || g._SYMBptr->sommet==at_superieur_strict || g._SYMBptr->sommet==at_superieur_egal || g._SYMBptr->sommet==at_inferieur_strict || g._SYMBptr->sommet==at_inferieur_egal || g._SYMBptr->sommet==at_and || g._SYMBptr->sommet==at_ou || g._SYMBptr->sommet==at_et || g._SYMBptr->sommet==at_not || g._SYMBptr->sommet==at_xor || g._SYMBptr->sommet==at_piecewise || g._SYMBptr->sommet==at_different
#ifndef FXCG
	|| g._SYMBptr->sommet==at_archive
#endif
	)
      return g;
    if (is_equal(g))
      return apply_to_equal(g,simplifier,contextptr);
    if (is_inf(g))
      return g;
    vecteur v(lvar(g)),w(v);
    for (unsigned i=0;i<w.size();++i){
      gen & wi=w[i];
      if (wi.type==_SYMB){
	gen f =wi._SYMBptr->feuille;
	if (wi.is_symb_of_sommet(at_pow) && f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->back().type==_FRAC){
	  gen base=f._VECTptr->front();
	  gen d= f._VECTptr->back()._FRACptr->den;
	  if ( (base.type==_INT_ && absint(base.val)>3) || base.type==_ZINT){
	    gen b=evalf_double(base,1,contextptr);
	    if (b.type==_DOUBLE_){
	      double bd=b._DOUBLE_val;
	      bd=std::log(bd);
	      int N=int(bd/std::log(2.0));
	      int ii;
	      for (ii=3;ii<=N;++ii){
		double di=std::exp(bd/ii);
		gen g=exact_double(di,1e-15);
		if (is_integer(g) && pow(g,ii,contextptr)==base){
		  wi=pow(g,ii*f._VECTptr->back(),contextptr);
		  break;
		}
	      }
	      if (ii!=N+1)
		continue;
	    }
	  }
	  if (d.type==_INT_){
	    gen f0=simplifier(f._VECTptr->front(),contextptr);
	    gen z=fast_icontent(f0);
	    gen n= f._VECTptr->back()._FRACptr->num;
	    if (d.val<0){ n=-n; d=-d;}
	    gen zn=pow(z,n,contextptr),a,b,fapprox;
	    bool pos; // pos should be true after next call since zn is > 0
	    zint2simpldoublpos(zn,a,b,pos,d.val,contextptr);
	    if (pos){
	      if (0 && n==1)
		wi=b*pow(fast_divide_by_icontent(f0,z/a),f._VECTptr->back(),contextptr);
	      else { // avoid extracting sqrt(2) out for simplify(exp(i*pi/5));
		if (d*f._VECTptr->back()==1 && has_evalf(f0,fapprox,1,contextptr))
		  wi=b*pow(a*fast_divide_by_icontent(f0,z),inv(d,contextptr),contextptr);
		else
		  wi=b*pow(a,inv(d,contextptr),contextptr)*pow(fast_divide_by_icontent(f0,z),f._VECTptr->back(),contextptr);
	      }
	      continue;
	    }
	  }
	}
	// check added for [] and () otherwise xcas_mode index shift is not taken in account
	if (wi._SYMBptr->sommet==at_at || wi._SYMBptr->sommet==at_of || wi._SYMBptr->sommet==at_Ans)
	  wi=symbolic(wi._SYMBptr->sommet,simplifier(f,contextptr));
	else
	  wi=wi._SYMBptr->sommet(simplifier(f,contextptr),contextptr);
      }
    }
    gen g_(g);
    if (v!=w)
      g_=subst(g,v,w,false,contextptr);
    g_=aplatir_fois_plus(g_);
    // distribute neg over +
    //vector< gen_op_context > negdistrib_v(1,negdistrib);
    //vector<const unary_function_ptr *> neg_v(1,at_neg);
    //g_=subst(g_,neg_v,negdistrib_v,false,contextptr);
    return liste2symbolique(symbolique2liste(g_,contextptr));
  }
  gen _simplifier(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (g.type<_IDNT) return g;
    if (is_equal(g))
      return apply_to_equal(g,_simplifier,contextptr);
    if (g.type!=_VECT)
      return simplifier(g,contextptr);
    return apply(g,_simplifier,contextptr);
  }

  gen _inferieur_strict_sort(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (a.type==_VECT && b.type==_VECT){
      unsigned as=unsigned(a._VECTptr->size()),bs=unsigned(b._VECTptr->size());
      for (unsigned i=0;i<as && i<bs;++i){
	if ((*a._VECTptr)[i]!=(*b._VECTptr)[i]){
	  a=(*a._VECTptr)[i]; b=(*b._VECTptr)[i];
	  break;
	}
      }
    }
    if (a.is_symb_of_sommet(at_equal) && b.is_symb_of_sommet(at_equal)){
      if (a._SYMBptr->feuille[0]!=b._SYMBptr->feuille[0]){
	a=a._SYMBptr->feuille[0]; b=b._SYMBptr->feuille[0];
      }
      else {
	a=a._SYMBptr->feuille[1]; b=b._SYMBptr->feuille[1];
      }
    }
    if (a.type==_STRNG){
      if (b.type!=_STRNG) return true;
      return *a._STRNGptr<*b._STRNGptr;
    }
    if (b.type==_STRNG)
      return false;
    gen res=inferieur_strict(a,b,contextptr);
    if (res.type==_INT_)
      return res;
    return islesscomplexthanf(a,b);
  }
  static const char _inferieur_strict_sort_s []="inferieur_strict_sort";
  static define_unary_function_eval (__inferieur_strict_sort,&_inferieur_strict_sort,_inferieur_strict_sort_s);
  define_unary_function_ptr5( at_inferieur_strict_sort ,alias_at_inferieur_strict_sort,&__inferieur_strict_sort,0,true);

#if 0
  void mysort(iterateur it,iterateur itend,const gen & f,GIAC_CONTEXT){
    int s=itend-it;
    for (;;){
      bool ok=true;
      for (int i=0;i<s-1;++i){
	if (!is_zero(f(makesequence(*(it+i+1),*(it+i)),contextptr))){
	  ok=false;
	  swapgen(*(it+i+1),*(it+i));
	}
      }
      if (ok)
	return;
    }
  }
#else
  void mysort(iterateur it,iterateur itend,const gen & f,GIAC_CONTEXT){
    sort(it,itend,gen_sort(f,contextptr));
  }
#endif

  gen _sort(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_SYMB)
      return simplifier(args,contextptr);
    if (args.type!=_VECT)
      return args; // FIXME sort in additions, symbolic(at_sort,args);
    vecteur v=*args._VECTptr;
    int subtype;
    gen f;
    bool usersort=v.size()==2 && v[0].type==_VECT && v[1].type!=_VECT
      // && args.subtype==_SEQ__VECT
      ;
    bool rev=false;
    if (usersort){
      f=v[1];
      subtype=v[0].subtype;
      v=*v[0]._VECTptr;
      if (is_equal(f) && f._SYMBptr->feuille[0]==at_reverse && !is_zero(f._SYMBptr->feuille[1])){
	f=at_inferieur_strict;
	rev=true;
      }
    }
    else {
      f=at_inferieur_strict_sort;
      subtype=args.subtype;
    }
    if (!v.empty() && (f==at_inferieur_strict || f==at_inferieur_strict_sort)){
      // check integer or double vector
      if (v.front().type==_INT_ && is_integer_vecteur(v,true)){
	// find min/max
	vector<int> w(vecteur_2_vector_int(v));
	int m=giacmin(w),M=giacmax(w);
	if (M-m<=int(w.size())/3){
	  vector<int> eff(M-m+1);
	  effectif(w,eff,m);
	  vecteur res(w.size());
	  iterateur it=res.begin();
	  int val=m;
	  for (unsigned i=0;i<eff.size();++val,++i){
	    unsigned I=eff[i];
	    for (unsigned j=0;j<I;++it,++j){
	      *it=val;
	    }
	  }
	  if (rev)
	    reverse(res.begin(),res.end());
	  return gen(res,subtype);
	}
	sort(w.begin(),w.end());
	vector_int2vecteur(w,v);
	if (rev)
	  reverse(v.begin(),v.end());
	return gen(v,subtype);
      }
      vector<giac_double> V;
      if (v.front().type==_DOUBLE_ && is_fully_numeric(v) && convert(v,V,true)){
	sort(V.begin(),V.end());
	v=vector_giac_double_2_vecteur(V);
	if (rev)
	  reverse(v.begin(),v.end());
	return gen(v,subtype);
      }
    }
    mysort(v.begin(),v.end(),f,contextptr);
    if (rev)
      reverse(v.begin(),v.end());
    return gen(v,subtype);
  }
  static const char _sort_s []="sort";
  static define_unary_function_eval (__sort,&_sort,_sort_s);
  define_unary_function_ptr5( at_sort ,alias_at_sort,&__sort,0,true);

  static const char _sorted_s []="sorted";
  static define_unary_function_eval (__sorted,&_sort,_sorted_s);
  define_unary_function_ptr5( at_sorted ,alias_at_sorted,&__sorted,0,true);

  static gen remove_nodisp(const gen & g){
    if (g.is_symb_of_sommet(at_nodisp))
      return g._SYMBptr->feuille;
    return g;
  }
  gen _ans(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()>1){
      vecteur v=*args._VECTptr;
      gen tmp=_ans(v.front(),contextptr);
      v.erase(v.begin());
      return _at(makesequence(tmp,gen(v,args.subtype)),contextptr);
    }
    int s=int(history_out(contextptr).size());
    if (!s)
      return undef;
    int i;
    if (args.type!=_INT_)
      i=-1;
    else {
      i=args.val;
      if (xcas_mode(contextptr)==3)
	i=-i;
    }
    if (i>=0){
      if (i>=s)
	return gentoofewargs(print_INT_(i));
      return remove_nodisp(history_out(contextptr)[i]);
    }
    if (s+i<0)
      return gentoofewargs(print_INT_(-i));
    return remove_nodisp(history_out(contextptr)[s+i]);
  }
  static const char _ans_s []="ans";
  static define_unary_function_eval (__ans,&_ans,_ans_s);
  define_unary_function_ptr5( at_ans ,alias_at_ans,&__ans,0,true);

  gen _quest(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (rpn_mode(contextptr))
      return gensizeerr(contextptr);
    int s=int(history_in(contextptr).size());
    if (!s)
      return undef;
    int i;
    if (args.type!=_INT_)
      i=-2;
    else
      i=args.val;
    if (i>=0){
      if (i>=s)
	return gentoofewargs(print_INT_(i));
      return remove_nodisp(history_in(contextptr)[i]);
    }
    if (s+i<0)
      return gentoofewargs(print_INT_(-i));
    return remove_nodisp(history_in(contextptr)[s+i]);
  }
  static const char _quest_s []="quest";
  static define_unary_function_eval (__quest,&_quest,_quest_s);
  define_unary_function_ptr5( at_quest ,alias_at_quest,&__quest,0,true);

  vector<int> float2continued_frac(double d_orig,double eps){
    if (eps<1e-11)
      eps=1e-11;
    double d=fabs(d_orig);
    vector<int> v;
    if (d>rand_max2){
#ifndef NO_STDEXCEPT
      setsizeerr(gettext("Float too large"));
#endif
      v.push_back(rand_max2);
      return v;
    }
    double i;
    for (;;){
      i=std::floor(d);
      v.push_back(int(i));
      d=d-i;
      if (d<eps)
	return v;
      d=1/d;
      eps=eps*d*d;
    }
  }

  gen continued_frac2gen(vector<int> v,double d_orig,double eps,GIAC_CONTEXT){
    gen res(v.back());
    for (;;){
      v.pop_back();
      if (v.empty()){
	if (
	    !my_isnan(d_orig) &&
	    fabs(evalf_double(res-d_orig,1,contextptr)._DOUBLE_val)>eps)
	  return d_orig;
	return res;
      }
      res=inv(res,contextptr);
      res=res+v.back();
    }
    return res;
  }

  gen chk_not_unit(const gen & g){
    if (g.is_symb_of_sommet(at_unit))
      return gensizeerr(gettext("Incompatible units"));
    return g;
  }

  gen convert_interval(const gen & g,int nbits,GIAC_CONTEXT){
#if defined HAVE_LIBMPFI && !defined NO_RTTI
    if (g.type==_VECT){
      vecteur res(*g._VECTptr);
      for (unsigned i=0;i<res.size();++i)
	res[i]=convert_interval(res[i],nbits,contextptr);
      return gen(res,g.subtype);
    }
    if (g.is_symb_of_sommet(at_rootof) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2){
      gen p=g._SYMBptr->feuille._VECTptr->front();
      gen x=g._SYMBptr->feuille._VECTptr->back();
      if (p.type==_VECT && x.type==_VECT){
	bool reel=is_real(*x._VECTptr,contextptr);
	// adjust (guess?) nbits
	gen g_=accurate_evalf(evalf_double(g,1,contextptr),60); // low prec multiprec evalf 
	vecteur P=*p._VECTptr;
	gen val=0;
	gen err=0; // absolute error
	gen absg=abs(g_,contextptr);
	gen rnd=accurate_evalf(gen(4e-15),60);
	for (int i=0;i<P.size();++i){
	  val = accurate_evalf(val*g_+P[i],60);
	  // 2* because we multiply complex numbers
	  err = 2*(err*absg+rnd*abs(val,contextptr));
	}
	err=err/abs(val,contextptr)/rnd; // relative error/rounding error
	if (is_strictly_greater(err,1,contextptr))
	  err=log(err,contextptr);
	else
	  err=0;
	double errd=evalf_double(err,1,contextptr)._DOUBLE_val;
	int nbitsmore=std::ceil(errd/std::log(2));
	if (nbits<56) 
	  nbits=56; // otherwise we should adjust precision 1e-14 in in_select_root call below
	gen r=complexroot(makesequence(symb_horner(*x._VECTptr,vx_var),pow(plus_two,-nbits-nbitsmore-4,contextptr)),true,contextptr);
	if (r.type==_VECT){
	  vecteur R=*r._VECTptr;
	  for (unsigned i=0;i<R.size();++i){
	    R[i]=R[i][0];
	  }
	  x=in_select_root(R,reel,contextptr,1e-14);
	  return horner(*p._VECTptr,x,contextptr);
	}
      }
    }
    if (g.type==_SYMB)
      return g._SYMBptr->sommet(convert_interval(g._SYMBptr->feuille,nbits,contextptr),contextptr);
    if (g.type==_REAL){
      if (dynamic_cast<real_interval *>(g._REALptr))
	return g;
    }
    if (g.type==_CPLX)
      return convert_interval(*g._CPLXptr,nbits,contextptr)+cst_i*convert_interval(*(g._CPLXptr+1),nbits,contextptr);
    if (is_integer(g) || g.type==_REAL){
      gen f=accurate_evalf(g,nbits);
      return eval(gen(makevecteur(f,f),_INTERVAL__VECT),1,contextptr);
    }
    if (g.type==_FRAC){
      return convert_interval(g._FRACptr->num,nbits,contextptr)/convert_interval(g._FRACptr->den,nbits,contextptr);
    }
    if (g.type==_IDNT){
      if (g==cst_pi || g==cst_euler_gamma){
	mpfi_t tmp;
	mpfi_init2(tmp,nbits);
	if (g==cst_pi)
	  mpfi_const_pi(tmp);
	else
	  mpfi_const_euler(tmp);
	gen res=real_interval(tmp);
	mpfi_clear(tmp);
	return res;
      }
    }
    return g;
#endif
    return gensizeerr("Interval arithmetic support not compiled. Please install MPFI and recompile");
  }

  gen convert_real(const gen & g,GIAC_CONTEXT){
#if defined HAVE_LIBMPFI && !defined NO_RTTI
    if (g.type==_STRNG)
      return convert_real(gen(*g._STRNGptr,contextptr),contextptr);
    if (g.type==_VECT){
      vecteur res(*g._VECTptr);
      for (unsigned i=0;i<res.size();++i)
	res[i]=convert_real(res[i],contextptr);
      return gen(res,g.subtype);
    }
    if (g.type==_SYMB)
      return g._SYMBptr->sommet(convert_real(g._SYMBptr->feuille,contextptr),contextptr);
    if (g.type==_REAL){
      if (dynamic_cast<real_interval *>(g._REALptr))
	return _milieu(g,contextptr);
    }
    if (g.type==_CPLX)
      return convert_real(*g._CPLXptr,contextptr)+cst_i*convert_real(*(g._CPLXptr+1),contextptr);
    return g;
#endif
    return gensizeerr("Interval arithmetic support not compiled. Please install MPFI and recompile");
  }

  gen os_nary_workaround(const gen & g){
#ifdef KHICAS
    if (g.type==_VECT && g._VECTptr->size()==1 && g._VECTptr->front().type==_VECT)
      return change_subtype(g._VECTptr->front(),_SEQ__VECT);
#endif
    return g;
  }

  gen denest_sto(const gen & g){
    if (g.type==_VECT){
      vecteur v(*g._VECTptr);
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it)
	*it=denest_sto(*it);
      return gen(v,g.subtype);
    }
    if (g.is_symb_of_sommet(at_sto) && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2){
      gen b=g._SYMBptr->feuille._VECTptr->front();
      gen a=g._SYMBptr->feuille._VECTptr->back();
      // a:=b, should be a0,..,an-2,an-1=b
      if (b.is_symb_of_sommet(at_sto)){
	// if b is a sto a0,...an-2,a1=c[0],c1,..,cn-1
	gen c=denest_sto(b);
	if (a.type==_VECT && c.type==_VECT){
	  vecteur av=*a._VECTptr;
	  vecteur cv=*c._VECTptr;
	  av.back()=symbolic(at_equal,makesequence(av.back(),cv.front()));
	  cv.erase(cv.begin());
	  return gen(mergevecteur(av,cv),_SEQ__VECT);
	}
      }
      if (a.type==_VECT){
	vecteur av=*a._VECTptr;
	av.back()=symbolic(at_equal,makesequence(av.back(),b));
	return gen(av,_SEQ__VECT);
      }
    }
    if (g.type==_SYMB){
      gen f=denest_sto(g._SYMBptr->feuille);
      return symbolic(g._SYMBptr->sommet,f);
    }
    return g;
  }

  gen re2zconj(const gen &g,GIAC_CONTEXT){
    return (g+symb_conj(g))/2;
  }
  
  gen im2zconj(const gen &g,GIAC_CONTEXT){
    return (g-symb_conj(g))/(2*cst_i);
  }
  
  gen abs2zconj(const gen &g,GIAC_CONTEXT){
    return symbolic(at_sqrt,g*symb_conj(g));
  }

  gen re2abs(const gen & g,GIAC_CONTEXT){
    return (g+pow(symb_abs(g),2,contextptr)/g)/2;    
  }
  
  gen im2abs(const gen & g,GIAC_CONTEXT){
    return (g-pow(symb_abs(g),2,contextptr)/g)/(2*cst_i);    
  }

  gen conj2abs(const gen &g,GIAC_CONTEXT){
    return pow(symb_abs(g),2,contextptr)/g;
  }

#ifndef NO_RTTI
  gen convert_gf(int a,galois_field * gfptr){
    galois_field gf(*gfptr);
    if (gf.a.type==_INT_){
      gf.a=a;
      return gf;
    }
    else if (gf.a.type==_VECT){
      vecteur v;
      int p=gf.p.val;
      for (;a;a/=p){
        v.push_back(a%p);
      }
      gf.a=v;
      return gf;
    }
    return undef;
  }
#endif
  
  gen _convert(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT){
      if (args.type==_POLY)
	return _convert(vecteur(1,args),contextptr);
      return gensizeerr(contextptr);
    }
    vecteur v=*args._VECTptr;
    int s=int(v.size());
    if ( (s==2 || s==3) && interval2realset(v[0]))
      v[0]=v[0][0];
    if ( (s==2 || (s==3 && v[1].type==_VECT)) && v[0].type==_VECT && v[s-1].subtype==_INT_MAPLECONVERSION && v[s-1].val==_REALSET__VECT){
      vecteur w;
      if (s==3)
        w=*v[1]._VECTptr;
      chk_set(w); // reorder
      vecteur vint(*v[0]._VECTptr);
      chk_set(vint);
      // check that vint is valid
      gen m(minus_inf);
      for (int i=0;i<vint.size();++i){
        gen cur=vint[i];
        if (cur.type!=_VECT || cur._VECTptr->size()!=2)
          return gensizeerr(contextptr);
        gen a=cur[0],b=cur[1];
        if (is_strictly_greater(m,a,contextptr) || is_strictly_greater(a,b,contextptr))
          return gensizeerr(contextptr);
        m=b;
      }
      v=makevecteur(vecteur(0),gen(vint,_SET__VECT),gen(w,_SET__VECT));
      return gen(v,_REALSET__VECT);
    }
    if (s==3 && v[0].type==_INT_ && v[0].subtype==_INT_PLOT && v[0].val==_AXES && v[2].type==_INT_ && v[2].subtype==_INT_MAPLECONVERSION && v[2].val==_SET__VECT){
      // axes.set(xlabel="",ylabel="")
      return v[1].type==_VECT?change_subtype(v[1],_SEQ__VECT):v[1];
    }
    if (v[0].type==_INT_ && v[0].subtype==0){
      // convert a small integer to a GF element or to a vector in Z/nZ or GF()
      // used only for small fields and small dim, assumes that
      // field or ring size^dim<2^31
      int a=v[0].val;
#ifndef NO_RTTI
      if (s==2){
        // convert(int,g) where g is a GF element (of a field with less than 2^31 elements)
        if (v[1].type==_USER){
          if (galois_field *gfptr=dynamic_cast<galois_field *>(v[1]._USERptr)){
            if (gfptr->p.type==_INT_)
              return convert_gf(a,gfptr);
          } // end GF
        } // end _USER
      } // end if s==2
#endif
      if (s==3 && v[2].type==_INT_ && v[2].val>=1){
        int dim=v[2].val;
        // convert(int,n,int dim) -> vector in (Z/nZ)^dim
        if (v[1].type==_INT_ && v[1].subtype==0 && v[1].val>1){
          int n=v[1].val;
          vecteur v(dim);
          for (int i=0;i<dim;++i){
            v[dim-1-i]=a%n;
            a/=n;
          }
          return v;
        }
#ifndef NO_RTTI
        // small field and small dim only
        if (v[1].type==_USER){
          if (galois_field *gfptr=dynamic_cast<galois_field *>(v[1]._USERptr)){
            if (gfptr->p.type==_INT_){
              int p=gfptr->p.val;
              int m=(gfptr->P.type==_VECT?gfptr->P._VECTptr->size():sizeinbase2(gfptr->P.val))-1;
              gen pm=pow((unsigned long)p,(unsigned long)m);
              if (pm.type==_INT_){
                int n=pm.val;
                vecteur v(dim);
                for (int i=0;i<dim;++i){
                  v[dim-1-i]=convert_gf(a%n,gfptr);
                  a/=n;
                }
                return v;
              }
            }
          }
        }
#endif
      }
    }
    if (s>=1 && v.front().type==_POLY){
      int dim=v.front()._POLYptr->dim;
      vecteur idx(dim);
      vector< monomial<gen> >::const_iterator it=v.front()._POLYptr->coord.begin(),itend=v.front()._POLYptr->coord.end();
      vecteur res;
      res.reserve(itend-it);
      for (;it!=itend;++it){
	index_t::const_iterator j=it->index.begin();
	for (int k=0;k<dim;k++,++j)
	  idx[k]=*j;
	res.push_back(makevecteur(it->value,idx));
      }
      return res;
    }
    if (s<2)
      return gensizeerr(contextptr);
    gen f=v[1];
    gen g=v.front();
    if (g.type==_VECT && v[1].type==_VECT)
      return gen(*g._VECTptr,v[1].subtype);
    if (f==at_cell && ckmatrix(g,true)){
      matrice m=makefreematrice(*g._VECTptr);
      int lignes,colonnes; mdims(m,lignes,colonnes);
      makespreadsheetmatrice(m,contextptr);
      if (s>=5){ // convert(matrix,command,row,col)
	// command==copy down, copy right, insert/delete row, insert/delete col
	gen CMD=v[2],cmd,R=v[3],C=v[4];
	if (!is_integer(R) || !is_integer(C))
	  return gensizeerr(contextptr);
	string scmd;
	if (CMD.type==_STRNG)
	  scmd=*CMD._STRNGptr;
	else {
	  if (!is_integral(CMD)) return gensizeerr(contextptr);
	  cmd=CMD.val;
	}
	if (scmd=="copy down")
	  cmd=0;
	if (scmd=="copy right")
	  cmd=1;
	if (scmd=="row+")
	  cmd=2;
	if (scmd=="row-")
	  cmd=3;
	if (scmd=="col+")
	  cmd=4;
	if (scmd=="col-")
	  cmd=5;
	int r=R.val,c=C.val;
	if (r<0 || r>=lignes || c<0 || c>=colonnes)
	  return gendimerr(contextptr);
	gen cellule=m[r][c];
	if (cmd==0){
	  for (int i=r+1;i<lignes;++i){
	    vecteur & v=*m[i]._VECTptr;
	    v[c]=freecopy(cellule);
	  }
	}
	if (cmd==1){
	  vecteur & v=*m[r]._VECTptr;
	  for (int j=c+1;j<colonnes;++j){
	    v[j]=freecopy(cellule);
	  }
	}
	if (cmd==2)
	  m=matrice_insert(m,r,c,1,0,undef,contextptr);
	if (cmd==3)
	  m=matrice_erase(m,r,c,1,0,contextptr);
	if (cmd==4)
	  m=matrice_insert(m,r,c,0,1,undef,contextptr);
	if (cmd==5)
	  m=matrice_erase(m,r,c,0,1,contextptr);
	spread_eval(m,contextptr);
      }
      return gen(m,_SPREAD__VECT);
    }
    if (f.is_symb_of_sommet(at_unit)){
      if (f._SYMBptr->feuille.type==_VECT && f._SYMBptr->feuille._VECTptr->size()==2)
	f=symbolic(at_unit,makesequence(1,f._SYMBptr->feuille._VECTptr->back()));
      g=chk_not_unit(mksa_reduce(evalf(g/f,1,contextptr),contextptr));
      g=evalf_double(g,1,contextptr);
      if (g.type!=_DOUBLE_ && g.type!=_CPLX && g.type!=_FLOAT_)
	return gensizeerr(gettext("Some units could not be converted to MKSA"));
      return g*f;
    }
    if (s==2 && f==at_conj){
      // convert re/im to conj
      vector<const unary_function_ptr *> vu;
      vu.push_back(at_re); 
      vu.push_back(at_im); 
      vu.push_back(at_abs); 
      vector <gen_op_context> vv;
      vv.push_back(re2zconj);
      vv.push_back(im2zconj);
      vv.push_back(abs2zconj);
      return subst(g,vu,vv,false,contextptr);
    }
    if (s==2 && f==at_abs){
      vector<const unary_function_ptr *> vu;
      vu.push_back(at_re); 
      vu.push_back(at_im); 
      vu.push_back(at_conj); 
      vector <gen_op_context> vv;
      vv.push_back(re2abs);
      vv.push_back(im2abs);
      vv.push_back(conj2abs);
      return subst(g,vu,vv,false,contextptr);
    }
    if (s==2 && f==at_interval)
      return convert_interval(g,int(decimal_digits(contextptr)*3.2),contextptr);
    if (s==2 && f==at_real && f.type==_FUNC)
      return convert_real(g,contextptr);
    if (s>=2 && f==at_series){
      if (g.type==_VECT){
	sparse_poly1 res=vecteur2sparse_poly1(*g._VECTptr);
	if (s>=3 && v[2].type==_INT_ && v[2].val>=0)
	  res.push_back(monome(undef,v[2].val));
	return res;
      }
      int s=series_flags(contextptr);
      series_flags(contextptr) = s | (1<<4);
      v.erase(v.begin()+1);
      gen res=_series(gen(v,args.subtype),contextptr);
      series_flags(contextptr) = s ;
      return res;
    }
    if (s==3 && f==at_interval && v[2].type==_INT_)
      return convert_interval(g,int(v[2].val*3.2),contextptr);
    if (s==3 && f.type==_INT_ ){
      if (f.val==_BASE && is_integer(v.back()) ){
	if (is_greater(1,v.back(),contextptr))
	  return gensizeerr(gettext("Bad conversion basis"));
	if (is_integer(g)){
	  if (is_zero(g))
	    return makevecteur(g);
	  // convert(integer,base,integer)
	  bool positif=is_positive(g,contextptr);
	  g=abs(g,contextptr);
	  vecteur res;
	  gen q;
	  for (;!is_zero(g);){
	    res.push_back(irem(g,v.back(),q));
	    g=q;
	  }
	  // reverse(res.begin(),res.end());
	  if (positif)
	    return res;
	  return -res;
	}
	if (g.type==_VECT){
	  vecteur w(*g._VECTptr);
	  reverse(w.begin(),w.end());
	  return horner(w,v.back());
	}
      }
      if (f.val==_CONFRAC && v.back().type==_IDNT){
	g=evalf_double(g,1,contextptr);
	if (g.type==_DOUBLE_)
	  return sto(vector_int_2_vecteur(float2continued_frac(g._DOUBLE_val,epsilon(contextptr))),v.back(),contextptr);
      }
    }
    if (s>2)
      g=gen(mergevecteur(vecteur(1,g),vecteur(v.begin()+2,v.begin()+s)),args.subtype);
    if (g.type==_STRNG){
      string s=*g._STRNGptr;
      int i=f.type==_INT_?f.val:-1;
      if (i==_IDNT)
	return identificateur(s);
      if (i==_INT_ || i==_ZINT || f==at_int){
	f=gen(s,contextptr);
	if (!is_integral(f)) 
	  return gensizeerr(contextptr);
	return f;
      }
      if (i==_SYMB || i==_FRAC)
	return gen(s,contextptr);
      if (i==_FRAC || f==at_frac){
	f=exact(gen(s,contextptr),contextptr);
	return f;
      }
      if (i==_REAL)
	return evalf(gen(s,contextptr),1,contextptr);
      if (i==_DOUBLE_ || f==at_real || f==at_float)
	return evalf_double(gen(s,contextptr),1,contextptr);
    }
#ifndef CAS38_DISABLED
    if (v[1].type==_FUNC){
      if (f==at_sincos)
	return sincos(g,contextptr);
      if (f==at_sin || f==at_SIN)
	return trigsin(g,contextptr);
      if (f==at_cos || f==at_COS)
	return trigcos(g,contextptr);
      if (f==at_tan || f==at_TAN)
	return halftan(g,contextptr);
      if (f==at_plus)
	return partfrac(tcollect(g,contextptr),true,contextptr);
      if (f==at_prod){
	if (is_integer(g)) return _ifactor(g,contextptr);
	return _factor(_texpand(g,contextptr),contextptr);
      }
      if (f==at_division)
	return _simplify(g,contextptr);
      if (f==at_exp || f==at_ln || f==at_EXP)
	return trig2exp(g,contextptr);
      if (f==at_string){
	int maxp=MAX_PRINTABLE_ZINT;
	MAX_PRINTABLE_ZINT= 1000000;
	gen res=string2gen(g.print(contextptr),false);
	MAX_PRINTABLE_ZINT=maxp;
	return res;
      }
      if (g.type==_MAP){
	if (f==at_matrix || f==at_vector){
	  if (g.subtype==_SPARSE_MATRIX)
	    *logptr(contextptr) << gettext("Run convert(matrix,array) for dense conversion") << '\n';
	  g.subtype=_SPARSE_MATRIX;
	  return g;
	}
	if (f==at_array){
	  vecteur res;
	  if (!convert(*g._MAPptr,res))
	    return gensizeerr(gettext("Invalid map"));
	  return res;
	}
	if (f==at_table){
	  g.subtype=0;
	  return g;
	}
      }
      if (g.type==_VECT){
	if (f==at_matrix){
	  if (!ckmatrix(g))
	    return gentypeerr(contextptr);
	  g.subtype=_MATRIX__VECT;
	  return g;
	}
	if ( f==at_vector || f==at_array){
	  g.subtype=_MATRIX__VECT;
	  return g;
	}
	if (f==at_seq){
	  g.subtype=_SEQ__VECT;
	  return g;
	}
	if (f==at_table){
	  const vecteur & m = *g._VECTptr;
	  // conversion to sparse matrix
	  gen_map M;
	  gen resg(M);
	  resg.subtype=_SPARSE_MATRIX;
	  gen_map & res=*resg._MAPptr;
	  convert(m,res);
	  return resg;
	}
      }
      return f(g,contextptr);
      // setsizeerr();
    }
#endif
    if (f.type==_INT_ && f.val>=0) {
      if (f.val==_CONFRAC){
	if (g.type==_VECT)
	  return _dfc2f(g,contextptr);
	g=evalf_double(g,1,contextptr);
	if (g.type==_DOUBLE_)
	  return vector_int_2_vecteur(float2continued_frac(g._DOUBLE_val,epsilon(contextptr)));
      }
      int i=f.val;
      if (f.val==_FRAC && f.subtype==_INT_TYPE)
	return exact(g,contextptr);
      if (f.val==_POLY1__VECT && f.subtype==_INT_MAPLECONVERSION){ // remove order_size
	if (g.type==_SPOL1){
	  vecteur v; int shift;
	  if (sparse_poly12vecteur(*g._SPOL1ptr,v,shift)){
	    if (is_undef(v.front()))
	      v.erase(v.begin());
	    v=trim(v,0);
	    if (shift)
	      return makesequence(gen(v,_POLY1__VECT),shift);
	    return gen(v,_POLY1__VECT);
	  }
	  return gensizeerr(contextptr);
	}
	if (g.type==_VECT && !g._VECTptr->empty()){
	  // check if g is a list of [coeff,[index]]
	  vecteur & w=*g._VECTptr;
	  if (w.front().type!=_VECT)
	    return change_subtype(g,_POLY1__VECT);
	  if (w.front().type==_VECT && w.front()._VECTptr->size()==2 && w.front()._VECTptr->back().type==_VECT){
	    unsigned dim=unsigned(w.front()._VECTptr->back()._VECTptr->size());
	    iterateur it=w.begin(),itend=w.end();
	    polynome res(dim);
	    vector< monomial<gen> > & coord =res.coord;
	    coord.reserve(itend-it);
	    index_t i(dim);
	    for (;it!=itend;++it){
	      if (it->type!=_VECT || it->_VECTptr->size()!=2 || it->_VECTptr->back().type!=_VECT)
		break;
	      vecteur & idx = *it->_VECTptr->back()._VECTptr;
	      if (idx.size()!=dim)
		break;
	      const_iterateur jt=idx.begin(),jtend=idx.end();
	      for (int k=0;jt!=jtend;++jt,++k){
		if (jt->type!=_INT_)
		  break;
		i[k]=jt->val;
	      }
	      if (jt!=jtend)
		break;
	      coord.push_back(monomial<gen>(it->_VECTptr->front(),i));
	    }
	    if (it==itend)
	      return res;
	  }
	}
	vecteur l(lop(g,at_order_size));
	vecteur lp(l.size(),zero);
	g=subst(g,l,lp,false,contextptr);
	return g;
      }
#ifndef CAS38_DISABLED
      if (f.subtype==_INT_MAPLECONVERSION){
	switch (i){
	case _TRIG:
	  return sincos(g,contextptr);
	case _EXPLN:
	  return trig2exp(g,contextptr);
	case _PARFRAC: case _FULLPARFRAC:
	  return _partfrac(g,contextptr);
	case _MAPLE_LIST:
	  if (g.subtype==0 && ckmatrix(g)){
	    vecteur v;
	    aplatir(*g._VECTptr,v);
	    return v;
	  }
	  g.subtype=0;
	  return g;
	case _SET__VECT:
	  if (g.type==_VECT){
	    vecteur v(*g._VECTptr);
	    comprim(v);
	    g=gen(v,_SET__VECT);
	    return g;
	  }
	default:
	  return gensizeerr(contextptr);
	}
      }
#endif
      g.subtype=v.back().val;
      return g;
    }
    return gensizeerr(contextptr);
  }
  static const char _convert_s []="convert";
  static define_unary_function_eval (__convert,&_convert,_convert_s);
  define_unary_function_ptr5( at_convert ,alias_at_convert,&__convert,0,true);

  static const char _convertir_s []="convertir";
  static define_unary_function_eval (__convertir,&_convert,_convertir_s);
  define_unary_function_ptr5( at_convertir ,alias_at_convertir,&__convertir,0,true);

  gen _deuxpoints(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_deuxpoints,args);
  }
  static const char _deuxpoints_s []=":";
  static define_unary_function_eval4 (__deuxpoints,&_deuxpoints,_deuxpoints_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_deuxpoints ,alias_at_deuxpoints ,&__deuxpoints);

#if defined FXCG || defined GIAC_HAS_STO_38
  gen _read(const gen & args,GIAC_CONTEXT){ return 0;}   
  gen _write(const gen & args,GIAC_CONTEXT){ return 0;}    
  static const char _read_s []="read";
  static define_unary_function_eval (__read,&_read,_read_s);
  define_unary_function_ptr5( at_read ,alias_at_read ,&__read,0,T_RETURN);

  static const char _write_s []="write";
  static define_unary_function_eval_quoted (__write,&_write,_write_s);
  define_unary_function_ptr5( at_write ,alias_at_write,&__write,_QUOTE_ARGUMENTS,true);

#else
  
#ifdef NSPIRE
  template<class T> void in_mws_translate(ios_base<T> &  inf,ios_base<T> &  of)
#else
  void in_mws_translate(istream & inf,ostream & of)
#endif
  {
    char c,oldc=0;
    // now read char by char, 
    for (;;){
      inf.get(c);
      if (c=='"')
	break;
    }
    for (;;){
      inf.get(c);
      if (c=='_'){
	of << '~';
	continue;
      }
      if (c==')' && oldc=='%') // %) -> % )
	of << " ";
      if (c=='"'){
	break;
      }
      if (c=='\n' || c==13)
	continue;
      if (c=='\\'){
	inf.get(c);
	if (c>'0' && c<='3'){ // read three chars -> octal code
	  unsigned char res=c-'0';
	  inf.get(c);
	  res <<= 3;
	  res += (c-'0');
	  inf.get(c);
	  res <<= 3;
	  res += (c-'0');
	  of << res;
	}
	else {
	  switch (c) {
	  case 'n':
	    of << '\n';
	    break;
	  case '+':
	    break;
	  case '"':
	    of << "\""; // Seems one " is needed, not two
	    // of << "\"\"";
	    break;
	  default:
	    of << c;
	  } // end switch
	} // end else octal code
	continue;
      } // end c==backslash
      else
	of << c;
      oldc=c;
    }
  }

  // Maple worksheet translate
#ifdef NSPIRE
  template<class T> void mws_translate(ios_base<T> &  inf,ios_base<T> &  of)
#else
  void mws_translate(istream & inf,ostream & of)
#endif
  {
    string thet;
    while (!inf.eof()){
      inf >> thet;
      int n1,n2,n3;
      n1=int(thet.size());
      if (n1>7 && thet.substr(n1-7,7)=="MPLTEXT"){
        inf >> n1 >> n2 >> n3;
	in_mws_translate(inf,of);	
	of << "\n";
      }
      else {
	if ( (n1>4 && thet.substr(n1-4,4)=="TEXT") || (n1>7 && thet.substr(n1-7,7)=="XPPEDIT") ){
	  inf >> n1 >> n2;
	  of << '"';
	  in_mws_translate(inf,of);
	  of << '"' << ";\n";
	}
      }
    }
  }

  // TI89/92 function/program translate
#if defined(WIN32) || defined(BESTA_OS) || defined(MS_SMART)
#define BUFFER_SIZE 16384
#endif

#ifdef NSPIRE
  template<class T> void ti_translate(ios_base<T> &  inf,ios_base<T> &  of)
#else
  void ti_translate(istream & inf,ostream & of)
#endif
  {
    char thebuf[BUFFER_SIZE];
    inf.getline(thebuf,BUFFER_SIZE,'\n');
    inf.getline(thebuf,BUFFER_SIZE,'\n');
    string lu=thebuf;
    lu=lu.substr(6,lu.size()-7);
    CERR << "Function name: " << lu << '\n';
    of << ":" << lu;
    inf.getline(thebuf,BUFFER_SIZE,'\n');  
    inf.getline(thebuf,BUFFER_SIZE,'\n');  
    of << thebuf << '\n';
    for (;inf.good();){
      inf.getline(thebuf,BUFFER_SIZE,'\n');
      lu=thebuf;
      if (lu=="\r")
        continue;
      if (lu=="\\STOP92\\\r"){
        break;
      }
      lu = tiasc_translate(lu);
      if (lu.size())
        of << ":" << lu << '\n';
    }
  }

  // FIXME SECURITY
  gen quote_read(const gen & args,GIAC_CONTEXT){
    if (args.type!=_STRNG)
      return symbolic(at_read,args);
    string fichier=*args._STRNGptr;
#ifdef KHICAS
    const char * s=read_file(fichier.c_str());
    if (!s)
      return undef;
    gen g(s,contextptr);
    return g;
#else // KHICAS
#if defined(EMCC) || defined(EMCC2)
    string s=fetch(fichier);
    return gen(s,contextptr);
#endif
    if (fichier.size()>4 && fichier.substr(0,4)=="http"){
      string s=fetch(fichier);
      return gen(s,contextptr);
    }
#ifdef NSPIRE
    file inf(fichier.c_str(),"r");
#else
    ifstream inf(fichier.c_str());
#endif
    if (!inf)
      return undef;
#if defined( VISUALC ) || defined( BESTA_OS )
    ALLOCA(char, thebuf, BUFFER_SIZE );// char * thebuf = ( char * )alloca( BUFFER_SIZE );
#else
    char thebuf[BUFFER_SIZE];
#endif
    inf.getline(thebuf,BUFFER_SIZE
#ifndef NSPIRE
		,'\n'
#endif
		);
    string lu(thebuf),thet;
    if (lu.size()>9 && lu.substr(0,9)=="{VERSION "){ // Maple Worksheet
#ifdef NSPIRE
      file of("__.map","w");
#else
      ofstream of("__.map");
#endif
      mws_translate(inf,of);
      of.close();
      xcas_mode(contextptr)=1;
      *logptr(contextptr) << gettext("Running maple text translation __.map") << '\n';
      fichier="__.map";
    }
    if (lu.size()>6 && lu.substr(0,6)=="**TI92"){ // TI archive
      inf.close();
      xcas_mode(contextptr)=3;
      eval(_unarchive_ti(args,contextptr),1,contextptr);
      return symbolic(at_xcas_mode,3);
    }
    if (lu=="\\START92\\\r"){ // TI text
#ifdef NSPIRE
      file of("__.ti","w");
#else
      ofstream of("__.ti");
#endif
      ti_translate(inf,of);
#ifndef NSPIRE
      of.close();
#endif
      xcas_mode(contextptr)=3;
      *logptr(contextptr) << gettext("Running TI89 text translation __.ti") << '\n';
      fichier="__.ti";
    } // end file of type TI
#ifdef NSPIRE
    file inf2(fichier.c_str(),"r");
#else
    inf.close();
    ifstream inf2(fichier.c_str());
#endif // NSPIRE
    vecteur v;
    readargs_from_stream(inf2,v,contextptr);
    return v.size()==1?v.front():gen(v,_SEQ__VECT);
#endif // KHICAS
  }
  bool is_address(const gen & g,size_t & addr){
    if (g.type==_INT_){
      addr=(g.val/4)*4; // align
      return true;
    }
    if (g.type!=_ZINT)
      return false;
    addr = modulo(*g._ZINTptr,(unsigned)0x80000000);
    addr = (addr/4)*4;
    addr += 0x80000000;
    return true;
  }
  gen _read(const gen & args,GIAC_CONTEXT){
#ifdef NUMWORKS
    if (inexammode()) return undef;
#else
#ifdef KHICAS
    if (exam_mode || nspire_exam_mode)
      return gensizeerr("Exam mode");
#endif
#endif
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    size_t addr;
    if (is_address(args,addr))
      return (int) *(unsigned char *) addr;
    if (args.type==_VECT && !args._VECTptr->empty() && args._VECTptr->front().type==_STRNG){
      string file=*args._VECTptr->front()._STRNGptr;
      if (file.size()>4 && file.substr(0,4)=="http"){
	string s=fetch(file);
	return string2gen(s,false);
      }
      FILE * f=fopen(file.c_str(),"r");
      if (!f)
	return undef;
      string s;
      while (!feof(f))
	s += char(fgetc(f));
      fclose(f);
      return string2gen(s,false);
    }
    if (args.type!=_STRNG)
      return symbolic(at_read,args);
    return eval(quote_read(args,contextptr),eval_level(contextptr),contextptr);
  }
  static const char _read_s []="read";
  static define_unary_function_eval (__read,&_read,_read_s);
  define_unary_function_ptr5( at_read ,alias_at_read ,&__read,0,T_RETURN);

  gen _read16(const gen & args,GIAC_CONTEXT){
#ifdef NUMWORKS
    if (inexammode()) return undef;
#endif
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    size_t addr;
    if (is_address(args,addr))
      return (int) *(unsigned short *) addr;
    if (args.type==_STRNG){
#ifdef KHICAS
      if (exam_mode || nspire_exam_mode)
	return gensizeerr("Exam mode");
#endif
      FILE * f=fopen(args._STRNGptr->c_str(),"r");
      if (!f)
	return undef;
      vecteur v,l; char buf[9]; buf[8]=0; int i;
      for (i=0;;++i){
	unsigned char c=fgetc(f);
	buf[i&7]=(c>=32 && c<=127)?c:'.';
	if (feof(f))
	  break;
	l.push_back(c);
	if ((i&0x7)==0x7){
	  l.insert(l.begin(),string2gen(buf,false));
	  l.insert(l.begin(),i-7);
	  v.push_back(l);
	  l.clear();
	}
      }
      if (!l.empty()){
	for (int j=l.size();j<8;++j)
	  l.push_back(-1);
	l.insert(l.begin(),string2gen(buf,false));
	l.insert(l.begin(),int(i&0xfffffff8));
	v.push_back(l);
      }
      fclose(f);
      return v;
    }
    return gensizeerr(contextptr);
  }
  static const char _read16_s []="read16";
  static define_unary_function_eval (__read16,&_read16,_read16_s);
  define_unary_function_ptr5( at_read16 ,alias_at_read16 ,&__read16,0,true);

  gen _read32(const gen & args,GIAC_CONTEXT){
#ifdef NUMWORKS
    if (inexammode()) return undef;
#endif
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    size_t addr;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->back().type==_INT_){
      int n=args._VECTptr->back().val;
      if (n<=0 || !is_address(args._VECTptr->front(),addr)) 
	return undef;
      vecteur res;
      for (int i=0;i<n;++i){
	res.push_back(makevecteur((longlong) addr,(longlong) *(unsigned *) addr));
	addr += 4;
      }
      return res;
    }
    if (is_address(args,addr))
      return (longlong) *(unsigned *) addr;
    return gensizeerr(contextptr);
  }
  static const char _read32_s []="read32";
  static define_unary_function_eval (__read32,&_read32,_read32_s);
  define_unary_function_ptr5( at_read32 ,alias_at_read32 ,&__read32,0,true);

  gen _addr(const gen & g,GIAC_CONTEXT){
    if (g.type==_VECT && g.subtype==_SEQ__VECT && g._VECTptr->size()==2){
      gen & obj=g._VECTptr->front();
      vecteur & ptr=*obj._VECTptr;
      return makevecteur((longlong) (&ptr),(int) taille(obj,RAND_MAX),tailles(obj));
    }
    vecteur & ptr=*g._VECTptr;
    return (longlong) (&ptr);
  }
  static const char _addr_s []="addr";
  static define_unary_function_eval (__addr,&_addr,_addr_s);
  define_unary_function_ptr5( at_addr ,alias_at_addr,&__addr,0,true);

#ifdef NSPIRE_NEWLIB
  gen _read_nand(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1)
      return  args;
    size_t addr;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->back().type==_INT_){
      int n=args._VECTptr->back().val;
      if (n<=0 || !is_address(args._VECTptr->front(),addr)) 
	return undef;
      // void read_nand(void* dest, int size, int nand_offset, int unknown, int percent_max, void* progress_cb); // terminer par (...,0,0,NULL)
      char * dest=(char *)malloc(4*n);
      read_nand(dest,4*n,addr,0,0,NULL);
      char buf[5]="aaaa";
      vecteur res;
      for (int i=0;i<n;++i,addr+=4,dest+=4){
	strncpy(buf,dest,4);
	res.push_back(makevecteur((longlong) addr,int(*dest),int(*(dest+1)),int(*(dest+2)),int(*(dest+3)),string2gen(buf,false)));
      }
      free(dest-4*n);
      return res;
    }
    if (is_address(args,addr))
      return _read_nand(makesequence(args,1),contextptr);
    return gensizeerr(contextptr);
  }
#else
  gen _read_nand(const gen & args,GIAC_CONTEXT){
    return undef;
  }
#endif
  static const char _read_nand_s []="read_nand";
  static define_unary_function_eval (__read_nand,&_read_nand,_read_nand_s);
  define_unary_function_ptr5( at_read_nand ,alias_at_read_nand ,&__read_nand,0,true);


  gen _write(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
#ifdef KHICAS
    return _ecris(args,contextptr);
#endif
    gen tmp=check_secure();
    if (is_undef(tmp)) return tmp;
    if (args.type==_VECT){
      vecteur v=*args._VECTptr;
      v.front()=eval(v.front(),eval_level(contextptr),contextptr);
      size_t addr;
      if (v.size()==2 && is_address(v.front(),addr)){
	gen vb=eval(v.back(),1,contextptr);
	if (vb.type==_INT_){
#ifdef KHICAS
	  if (exam_mode)
	    return gensizeerr("Exam mode");
#endif
	  unsigned char * ptr =(unsigned char *) addr;
	  // int res=*ptr;
	  *ptr=vb.val;
	  return *ptr;//return res;
	}
      }
      if (v.size()<2 || v.front().type!=_STRNG)
	return gensizeerr(contextptr);
      if (v.size()==2 && is_zero(v[1])){
	v[1]=_VARS(0,contextptr);
	if (v[1].type==_VECT)
	  v=mergevecteur(vecteur(1,v[0]),*v[1]._VECTptr);
      }
#ifdef NSPIRE
      file inf(v[0]._STRNGptr->c_str(),"w");
#else
      ofstream inf(v[0]._STRNGptr->c_str());
#endif
      const_iterateur it=v.begin()+1,itend=v.end();
      for (;it!=itend;++it){
	gen tmp=eval(*it,1,contextptr);
	if (it->type==_IDNT){
	  gen tmp2=*it;
	  if (tmp.type==_USER)
	    tmp=tmp._USERptr->giac_constructor(contextptr);
	  inf << symb_sto(tmp,tmp2) << ";" << '\n';
	}
	else
	  inf << tmp << ";" << '\n';
      }
      return plus_one;
    }
    if (args.type!=_STRNG)
      return symbolic(at_write,args);
#if !defined KHICAS && !defined SDL_KHICAS
    if (turtle_stack(contextptr).size()>1)
      return _ecris(args,contextptr);
#endif
#ifdef NSPIRE
    file inf(args._STRNGptr->c_str(),"w");
#else
    ofstream inf(args._STRNGptr->c_str());
#endif
    const_iterateur it=history_in(contextptr).begin(),itend=history_in(contextptr).end();
    if (it==itend)
      return zero;
    for (;it!=itend;++it){
      if (!it->is_symb_of_sommet(at_write))
	inf << *it << ";" << '\n';
    }
    return plus_one;
  }
  static const char _write_s []="write";
  static define_unary_function_eval_quoted (__write,&_write,_write_s);
  define_unary_function_ptr5( at_write ,alias_at_write,&__write,_QUOTE_ARGUMENTS,true);

  gen _write32(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
#ifdef KHICAS
    if (exam_mode || nspire_exam_mode)
      return gensizeerr("Exam mode");
#endif
    if (args.type!=_VECT)
      return _read32(args,contextptr);
    if (args.type==_VECT){
      vecteur v=*args._VECTptr;
      if (v.empty()) return gensizeerr(contextptr);
      gen vf=v.front(),vb=v.back();
      size_t addr;
      if (v.size()==2 && is_address(vf,addr)){
	unsigned * ptr =(unsigned *) addr;
	if (vb.type==_INT_){
	  *ptr=vb.val;
	  return makevecteur(longlong(addr),longlong(*ptr));
	}
	if (vb.type==_ZINT){
	  unsigned l =mpz_get_si(*vb._ZINTptr);
	  *ptr=l;
	  return makevecteur(longlong(addr),longlong(*ptr));
	}
      }
    }
    return gensizeerr(contextptr);
  }
  static const char _write32_s []="write32";
  static define_unary_function_eval (__write32,&_write32,_write32_s);
  define_unary_function_ptr5( at_write32 ,alias_at_write32,&__write32,0,true);

  gen _write16(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
#ifdef KHICAS
    if (exam_mode || nspire_exam_mode)
      return gensizeerr("Exam mode");
#endif
    if (args.type==_VECT){
      vecteur v=*args._VECTptr;
      size_t addr;
      if (v.size()==2 && is_address(v.front(),addr)){
	gen vb=v.back();
	unsigned short * ptr =(unsigned short *) addr;
	if (vb.type==_INT_){
	  *ptr=vb.val;
	  return int( (unsigned short) vb.val);
	}
      }
    }
    return gensizeerr(contextptr);
  }
  static const char _write16_s []="write16";
  static define_unary_function_eval (__write16,&_write16,_write16_s);
  define_unary_function_ptr5( at_write16 ,alias_at_write16,&__write16,0,true);

  gen _save_history(const gen & args,GIAC_CONTEXT){
#ifdef NSPIRE
    return 0;
#else
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen tmp=check_secure();
    if (is_undef(tmp)) return tmp;
    if (args.type!=_STRNG)
      return symbolic(at_save_history,args);
    ofstream of(args._STRNGptr->c_str());
    vecteur v(history_in(contextptr));
    if (!v.empty() && v.back().is_symb_of_sommet(at_save_history))
      v.pop_back();
    of << gen(history_in(contextptr),_SEQ__VECT) << '\n';
    return plus_one;
#endif
  }
  static const char _save_history_s []="save_history";
  static define_unary_function_eval (__save_history,&_save_history,_save_history_s);
  define_unary_function_ptr5( at_save_history ,alias_at_save_history,&__save_history,0,true);

#endif // FXCG
  /*
  gen _matrix(const gen & args){
  if ( args){
    if (!ckmatrix(args))
      return symbolic(at_matrix.type==_STRNG &&  args.subtype==-1{
    if (!ckmatrix(args))
      return symbolic(at_matrix)) return  args){
    if (!ckmatrix(args))
      return symbolic(at_matrix;
    if (!ckmatrix(args))
      return symbolic(at_matrix,args);
    gen res=args;
    res.subtype=_MATRIX__VECT;
    return res;
  }
  static const char _matrix_s []="matrix";
  static define_unary_function_eval (__matrix,&_matrix,_matrix_s);
  define_unary_function_ptr5( at_matrix ,alias_at_matrix,&__matrix);
  */

  gen symb_findhelp(const gen & args){
    return symbolic(at_findhelp,args);
  }
  gen _findhelp(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen args(g);
    int lang=language(contextptr);
    int helpitems = 0;
    if (g.type==_VECT && g.subtype==_SEQ__VECT && g._VECTptr->size()==2 && g._VECTptr->back().type==_INT_){
      args=g._VECTptr->front();
      lang=absint(g._VECTptr->back().val);
    }
#ifdef HAVE_LIBPARI
    if (args.type==_FUNC && string(args._FUNCptr->ptr()->s)=="pari")
      return string2gen(pari_help(0),false);
    if (args.type==_SYMB && string(args._SYMBptr->sommet.ptr()->s)=="pari")
      return string2gen(pari_help(args._SYMBptr->feuille),false);
#endif
    if (args.type==_IDNT)
      args=eval(args,1,contextptr);
    if (args.type==_SYMB && args._SYMBptr->sommet==at_of && args._SYMBptr->feuille.type==_VECT && args._SYMBptr->feuille._VECTptr->size()==2){
      args=args._SYMBptr->feuille._VECTptr->front();
      args=eval(args,1,contextptr);
    }
    if (args.type==_SYMB && args._SYMBptr->sommet!=at_of){
      if (args._SYMBptr->sommet==at_program && args._SYMBptr->feuille.type==_VECT){
	vecteur v=*args._SYMBptr->feuille._VECTptr;
	if (v.size()==3){
	  string argss="Help on user function "+g.print(contextptr)+"(";
	  if (v[0].type==_VECT && v[0].subtype==_SEQ__VECT && v[0]._VECTptr->size()==1)
	    argss += v[0]._VECTptr->front().print(contextptr);
	  else
	    argss += v[0].print(contextptr);
	  gen g=v[2];
	  while (g.is_symb_of_sommet(at_bloc)) 
	    g=g._SYMBptr->feuille;
	  while (g.is_symb_of_sommet(at_local))
	    g=g._SYMBptr->feuille[1];
	  while (g.type==_VECT && !g._VECTptr->empty())
	    g=g._VECTptr->front();
	  argss += ")\n";
	  if (g.type==_STRNG)
	    argss += *g._STRNGptr;
	  else {
	    argss += "Begins by: ";
	    argss +=g.print(contextptr);
	  }
	  return string2gen(argss,false);
	}
      }
      args=args._SYMBptr->sommet;
    }
    string argss=args.print(contextptr);
    // remove space at the end if required
    while (!argss.empty() && argss[argss.size()-1]==' ')
      argss=argss.substr(0,argss.size()-1);
#ifdef HAVE_LIBPARI
    if (argss.size()>5 && argss.substr(0,5)=="pari_")
      return string2gen(pari_help(string2gen(argss.substr(5,argss.size()-5),false)),false);      
#endif
    const char *cmdname=argss.c_str(),* howto=0, * syntax=0, * related=0, *examples=0;
    if (has_static_help(cmdname,lang,howto,syntax,examples,related)){
#if defined(EMCC) || defined(EMCC2)
      if (argss.size()>2 && argss[0]=='\'' && argss[argss.size()-1]=='\'')
	argss=argss.substr(1,argss.size()-2);
      // should split related at commas, and display buttons
      // should split examples at semis, and display buttons
      string res="<b>"+argss+"</b> : "+string(howto)+"<br>";
      if (strlen(syntax))
	res = res+argss+'('+string(syntax)+")<br>";
      else
	res = res+"<br>";
      res = res+string(related)+"<br>";
      if (strlen(examples))
	res = res +string(examples);
      return string2gen(res,false);
#endif
#ifdef NSPIRE
      if (argss.size()>2 && argss[0]=='\'' && argss[argss.size()-1]=='\'')
	argss=argss.substr(1,argss.size()-2);
      COUT << howto << '\n' << "Syntax: " << argss << "(" << syntax << ")" << '\n' << "See also: " << related << '\n' ;
      COUT << "Examples: " << examples << '\n';
      return 1;
#else
      string related_str(related);
      for (int i=related_str.size();i-->0;) {
	if (related_str[i]==',')
	  related_str.insert(related_str.begin()+i+1,' ');
      }
      string examples_str(examples);
      for (int i=examples_str.size();i-->0;) {
	if (i>0 && examples_str.substr(i-1,2)==";\\")
	  examples_str.replace(i-1,2,"\n");
      }
      string helptext;
      if (howto!=NULL && strlen(howto)>0)
        helptext = string("Description: ") + howto + "\n";
      if (strlen(syntax)>0)
        helptext += "Usage: " + string(cmdname).substr(1,strlen(cmdname)-2) + "(" + syntax + ")\n";
      if (related_str.size()>0)
        helptext += "Related: " + related_str + "\n";
      if (examples_str.size()>0)
        helptext += "Examples:\n" + examples_str;
      return string2gen(helptext,false);
      // return string2gen(string(howto?howto:"")+'\n'+string(syntax)+'\n'+string(related)+'\n'+string(examples),false);
#endif
    }
#ifndef GIAC_HAS_STO_38
    if (!vector_aide_ptr() || vector_aide_ptr()->empty()){
      if (!vector_aide_ptr())
	vector_aide_ptr() = new vector<aide>;
      * vector_aide_ptr()=readhelp("aide_cas",helpitems,false);
      if (!helpitems){
	* vector_aide_ptr()=readhelp(default_helpfile,helpitems);
      }
      if (!helpitems){
	* vector_aide_ptr()=readhelp((giac_aide_dir()+"aide_cas").c_str(),helpitems);
      }
    }
#endif
    if (vector_aide_ptr()){
      string s=argss; // args.print(contextptr);
      int l=int(s.size());
      if ( (l>2) && (s[0]=='\'') && (s[l-1]=='\'') )
	s=s.substr(1,l-2);
      l=int(s.size());
      if (l && s[l-1]==')'){
	int i=l-1;
	for (;i;--i){
	  if (s[i]=='(')
	    break;
	}
	if (i)
	  s=s.substr(0,i);
      }
      s=writehelp(helpon(s,*vector_aide_ptr(),lang,int(vector_aide_ptr()->size())),lang);
      return string2gen(s,false);
    }
    else
      return gensizeerr(gettext("No help file found"));
    // return 0;
  }
  static const char _findhelp_s []="findhelp";
  static define_unary_function_eval_quoted (__findhelp,&_findhelp,_findhelp_s);
  define_unary_function_ptr5( at_findhelp ,alias_at_findhelp,&__findhelp,_QUOTE_ARGUMENTS,true);

  static const char _help_s []="help";
  static define_unary_function_eval_quoted (__help,&_findhelp,_help_s);
  define_unary_function_ptr5( at_help ,alias_at_help,&__help,_QUOTE_ARGUMENTS,true);

  gen _member(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen g=args;
    vecteur v;
    if (args.type!=_VECT){
      g=args.eval(eval_level(contextptr),contextptr);
      if (g.type!=_VECT)
	return symbolic(at_member,args);
      v=*g._VECTptr;
    }
    else {
      v=*args._VECTptr;
      if (v.size()>1){
	v[0]=eval(v[0],eval_level(contextptr),contextptr);
	v[1]=eval(v[1],eval_level(contextptr),contextptr);
      }
    }
    int s=int(v.size());
    if (s<2)
      return gentoofewargs("");
    int i=-1;
    if (v[1].type==_MAP){
      const gen_map & m=*v[1]._MAPptr;
      gen_map::const_iterator it=m.find(v[0]),itend=m.end();
      return change_subtype(it!=itend,_INT_BOOLEAN);
    }
    if (v[0].type==_STRNG && v[1].type==_STRNG){
      string f=*v[0]._STRNGptr,s=*v[1]._STRNGptr;
      int pos=s.find(f);
      if (pos<0 || pos>=s.size())
	i=0;
      else
	i=pos+1;
    }
    else {
      if (v[1].type!=_VECT)
	return gensizeerr(contextptr);
      i=equalposcomp(*v[1]._VECTptr,v[0]);
    }
    if (s==3){
      gen tmpsto;
      if (array_start(contextptr))
	tmpsto=sto(i,v[2],contextptr);
      else
	tmpsto=sto(i-1,v[2],contextptr);
      if (is_undef(tmpsto)) return tmpsto;
    }
    return i;
  }
  static const char _member_s []="member";
  static define_unary_function_eval_quoted (__member,&_member,_member_s);
  define_unary_function_ptr5( at_member ,alias_at_member,&__member,_QUOTE_ARGUMENTS,true);

  // tablefunc(expression,[var,min,step])
  gen _tablefunc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen f,x=vx_var,xstart=gnuplot_xmin,step=(gnuplot_xmax-gnuplot_xmin)/10;
    gen xmax=gnuplot_xmax;
    if (args.type==_VECT){
      vecteur & v=*args._VECTptr;
      int s=int(v.size());
      if (!s)
	return gentoofewargs("");
      f=v[0];
      if (s>1)
	x=v[1];
      if (s>2)
	xstart=v[2];
      if (s>3)
	step=v[3];
      if (s>4)
	xmax=v[4];
    }
    else
      f=args;
    if (f.is_symb_of_sommet(at_program)){
      f=f[3];
      x=f[1];
    }
    vecteur l0(makevecteur(x,f));
    gen graphe=symbolic(at_plotfunc,
			gen(makevecteur(_cell(makevecteur(vecteur(1,minus_one),vecteur(1,zero)),contextptr),
					symb_equal(_cell(makevecteur(vecteur(1,minus_one),vecteur(1,minus_one)),contextptr),symb_interval(xstart,xmax))
#ifdef NUMWORKS
					,symb_equal(change_subtype(_NSTEP,_INT_PLOT),100)
#endif
				    ),_SEQ__VECT));
    graphe.subtype=_SPREAD__SYMB;
    vecteur l1(makevecteur(step,graphe));
    gen l31(_cell(makevecteur(vecteur(1,minus_one),vecteur(1,zero)),contextptr)+_cell(makevecteur(plus_one,vecteur(1,zero)),contextptr));
    l31.subtype=_SPREAD__SYMB;
    gen l32(symb_evalf(symbolic(at_subst,gen(makevecteur(_cell(makevecteur(zero,vecteur(1,zero)),contextptr),_cell(makevecteur(zero,vecteur(1,minus_one)),contextptr),_cell(makevecteur(vecteur(1,zero),vecteur(1,minus_one)),contextptr)),_SEQ__VECT))));
    l32.subtype=_SPREAD__SYMB;
    vecteur l2(makevecteur(xstart,l32));
    vecteur l3(makevecteur(l31,l32));
    return makevecteur(l0,l1,l2,l3);
  }
  static const char _tablefunc_s []="tablefunc";
  static define_unary_function_eval (__tablefunc,&_tablefunc,_tablefunc_s);
  define_unary_function_ptr5( at_tablefunc ,alias_at_tablefunc,&__tablefunc,0,true);

  // tableseq(expression,[var,value])
  // var is a vector of dim the number of terms in the recurrence
  gen _tableseq(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    gen f,x=vx_var,uzero=zero;
    int dim=1;
    double xmin=gnuplot_xmin,xmax=gnuplot_xmax;
    if (args.type==_VECT){
      vecteur & v=*args._VECTptr;
      int s=int(v.size());
      if (!s)
	return gentoofewargs("");
      f=v[0];
      if (s>1)
	x=v[1];
      if (s>2)
	uzero=evalf_double(v[2],1,contextptr);
      if (x.type==_VECT){
	dim=int(x._VECTptr->size());
	if (uzero.type!=_VECT)
	  return gentypeerr(contextptr);
	if (uzero._VECTptr->front().type==_VECT)
	  uzero=uzero._VECTptr->front();
	if ( (uzero.type!=_VECT) || (signed(uzero._VECTptr->size())!=dim) )
	  return gendimerr(contextptr);
      }
      else {
	if (uzero.type==_VECT && uzero._VECTptr->size()==3){
	  vecteur & uv=*uzero._VECTptr;
	  if (uv[1].type!=_DOUBLE_ || uv[2].type!=_DOUBLE_)
	    return gensizeerr(contextptr);
	  xmin=uv[1]._DOUBLE_val;
	  xmax=uv[2]._DOUBLE_val;
	  uzero=uv[0];
	}
      }
    }
    else
      f=args;
    if (f.is_symb_of_sommet(at_program)){
      f=f[3];
      x=f[1];
    }
    vecteur res;
    res.push_back(f);
    if (x.type!=_VECT){
      res.push_back(x);
      if (dim!=1)
	res.push_back(dim);
      else {
	gen l31(symbolic(at_plotseq,
		       gen(makevecteur(
				       _cell(makevecteur(zero,vecteur(1,zero)),contextptr),
				       symb_equal(_cell(makevecteur(vecteur(1,-1),vecteur(1,zero)),contextptr),makevecteur(_cell(makevecteur(vecteur(1,plus_one),vecteur(1,zero)),contextptr),xmin,xmax)),
				       //symb_equal(_cell(makevecteur(plus_one,vecteur(1,zero)),contextptr),makevecteur(_cell(makevecteur(vecteur(1,plus_one),vecteur(1,zero)),contextptr),xmin,xmax)),
				       9,at_tableseq),_SEQ__VECT
			   )
		       )
	      );
	l31.subtype=_SPREAD__SYMB;
	res.push_back(l31);
      }
      res.push_back(uzero);
      gen l51(symb_evalf(symbolic(at_subst,gen(makevecteur(_cell(makevecteur(zero,vecteur(1,zero)),contextptr),_cell(makevecteur(plus_one,vecteur(1,zero)),contextptr),_cell(makevecteur(vecteur(1,minus_one),vecteur(1,zero)),contextptr)),_SEQ__VECT))));
      l51.subtype=_SPREAD__SYMB;
      res.push_back(l51);
    }
    else {
      for (int i=0;i<dim;++i)
	res.push_back(x[i]);
      vecteur tmp1,tmp2;
      for (int i=0;i<dim;++i){
	res.push_back(uzero[i]);
	tmp1.push_back(_cell(makevecteur(i+1,vecteur(1,zero)),contextptr));
	tmp2.push_back(_cell(makevecteur(vecteur(1,i-dim),vecteur(1,zero)),contextptr));
      }
      gen l41(symb_eval(symbolic(at_subst,gen(makevecteur(_cell(makevecteur(zero,vecteur(1,zero)),contextptr),tmp1,tmp2),_SEQ__VECT))));
      l41.subtype=_SPREAD__SYMB;
      res.push_back(l41);
    }
    return mtran(vecteur(1,res));
  }
  static const char _tableseq_s []="tableseq";
  static define_unary_function_eval_quoted (__tableseq,&_tableseq,_tableseq_s);
  define_unary_function_ptr5( at_tableseq ,alias_at_tableseq,&__tableseq,_QUOTE_ARGUMENTS,true);

  gen protectevalorevalf(const gen & g,int level,bool approx,GIAC_CONTEXT){
    gen res;
#ifdef HAVE_LIBGSL //
    gsl_set_error_handler_off();
#endif //
    ctrl_c = false; interrupted=false;
    // save cas_setup in case of an exception
    vecteur cas_setup_save = cas_setup(contextptr);
    if (cas_setup_save.size()>5 && cas_setup_save[5].type==_VECT && cas_setup_save[5]._VECTptr->size()==2){
      vecteur & v = *cas_setup_save[5]._VECTptr;
      if (is_strictly_greater(v[0],1e-6,contextptr)){
	*logptr(contextptr) << gettext("Restoring epsilon to 1e-6 from ") << v[0] << '\n';
	epsilon(1e-6,contextptr);
      }
      if (is_strictly_greater(v[1],1e-6,contextptr)){
	*logptr(contextptr) << gettext("Restoring proba epsilon to 1e-6 from ") << v[0] << '\n';
	proba_epsilon(contextptr)=1e-6;
      }
      cas_setup_save=cas_setup(contextptr);
    }
    debug_struct dbg;
    dbg=*debug_ptr(contextptr);
#ifndef NO_STDEXCEPT
    try {
#endif
      res=approx?g.evalf(level,contextptr):g.eval(level,contextptr);
#ifndef NO_STDEXCEPT
    }
    catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      *debug_ptr(contextptr)=dbg;
      res=string2gen(e.what(),false);
      res.subtype=-1;
      ctrl_c=false; interrupted=false;
      // something went wrong, so restore the old cas_setup
      cas_setup(cas_setup_save, contextptr);
    }
#endif
    return res;
  }

  gen protectevalf(const gen & g,int level, GIAC_CONTEXT){
    return protectevalorevalf(g,level,true,contextptr);
  }

  gen protecteval(const gen & g,int level, GIAC_CONTEXT){
    return protectevalorevalf(g,level,approx_mode(contextptr),contextptr);
  }

  static string printasnodisp(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    int maplemode=xcas_mode(contextptr) & 0x07;
    if (maplemode==1 || maplemode==2){
      string res=feuille.print(contextptr);
      int l=int(res.size()),j;
      for (j=l-1;j>=0 && res[j]==' ';--j)
	;
      if (res[j]==';')
	res[j]=':';
      else
	res += ':';
      return res;
    }
    return sommetstr+("("+feuille.print(contextptr)+")");
  }
  gen _nodisp(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return string2gen("Done",false);
  }
  static const char _nodisp_s []="nodisp";
  static define_unary_function_eval2 (__nodisp,(const gen_op_context)_nodisp,_nodisp_s,&printasnodisp);
  define_unary_function_ptr5( at_nodisp ,alias_at_nodisp,&__nodisp,0,true);

  gen _unapply(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || args._VECTptr->empty() )
      return gentypeerr(contextptr);
    vecteur v=*args._VECTptr,w;
    int s=int(v.size());
    if (s<2)
      w=vecteur(1,vx_var);
    else {
      if (s==2 && v[1].type==_VECT)
	w=*v[1]._VECTptr;
      else
	w=vecteur(v.begin()+1,v.end());
    }
    gen g=subst(v[0].eval(eval_level(contextptr),contextptr),w,w,false,contextptr);
    if (g.type==_VECT && !g.subtype)
      g=makevecteur(g);
    return symbolic(at_program,gen(makevecteur(gen(w,_SEQ__VECT),w*zero,g),_SEQ__VECT));
  }
  static const char _unapply_s []="unapply";
  static define_unary_function_eval_quoted (__unapply,&_unapply,_unapply_s);
  define_unary_function_ptr5( at_unapply ,alias_at_unapply,&__unapply,_QUOTE_ARGUMENTS,true);

  gen _makevector(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return vecteur(1,args);
    vecteur & v=*args._VECTptr;
    if (ckmatrix(args))
      return gen(v,_MATRIX__VECT);
    return v;
  }
  static const char _makevector_s []="makevector";
  static define_unary_function_eval (__makevector,&_makevector,_makevector_s);
  define_unary_function_ptr5( at_makevector ,alias_at_makevector,&__makevector,0,true);


  gen _makesuite(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return vecteur(1,args);
    vecteur & v=*args._VECTptr;
    return gen(v,_SEQ__VECT);
  }
  static const char _makesuite_s []="makesuite";
  static define_unary_function_eval (__makesuite,&_makesuite,_makesuite_s);
  define_unary_function_ptr5( at_makesuite ,alias_at_makesuite,&__makesuite,0,true);

  gen _matrix(const gen & g,const context * contextptr){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (g.type==_MAP){
      vecteur res;
      if (!convert(*g._MAPptr,res))
	return gensizeerr(contextptr);
      return res;
    }
    if (g.type==_INT_) return _makemat(g,contextptr);
    if (g.type!=_VECT)
      return gentypeerr(contextptr);
    vecteur v=*g._VECTptr;
    if (ckmatrix(v))
      return gen(v,_MATRIX__VECT);
    int vs=int(v.size());
    if (vs<2)
      return gentypeerr(contextptr);
    if (vs==2 && v[0].type==_INT_ && v[1].type==_VECT){
      int l=giacmax(v[0].val,0);
      vecteur res(l);
      vecteur w(*v[1]._VECTptr);
      if (ckmatrix(w))
	aplatir(*v[1]._VECTptr,w);
      int s=giacmin(l,int(w.size()));
      for (int i=0;i<s;++i)
	res[i]=w[i];
      return gen(res,_MATRIX__VECT);
    }
    if (vs==2){
      v.push_back(zero);
      ++vs;
    }
    if ( (v[0].type!=_INT_) || (v[1].type!=_INT_) )
      return gensizeerr(contextptr);
    if (v[0].val==0)
      return symbolic(at_matrix,g); // used by PARI in bnfinit
    int l(giacmax(v[0].val,1)),c(giacmax(v[1].val,1));
    if (l*longlong(c)>LIST_SIZE_LIMIT)
      return gendimerr(contextptr);
    if (vs==3 && v[2].type<=_IDNT){
      vecteur res(l);
      for (int i=0;i<l;++i)
	res[i]=vecteur(c,v[2]);
      return gen(res,_MATRIX__VECT);
    }
    bool transpose=(vs>3);
    if (transpose){ // try to merge arguments there
      // v[2]..v[vs-1] represents flattened submatrices 
      vecteur v2;
      for (int i=2;i<vs;++i){
	if (v[i].type!=_VECT)
	  return gentypeerr(contextptr);
	vecteur & w = *v[i]._VECTptr;
	int vis=int(w.size());
	if (vis % l)
	  return gendimerr(contextptr);
	int nc=vis/l;
	for (int J=0;J<nc;++J){
	  for (int I=J;I<vis;I+=nc)
	    v2.push_back(w[I]);
	}
      }
      v[2]=v2;
      swapint(l,c);
    }
    if (v[2].type==_VECT){
      vecteur w=*v[2]._VECTptr;
      if (is_undef(w)) return gensizeerr(contextptr); // w.clear();
      int s=int(w.size());
      if (ckmatrix(w)){
	int ss=0;
	if (s)
	  ss=int(w[0]._VECTptr->size());
	int ll=giacmin(l,s);
	for (int i=0;i<ll;++i){
	  if (ss<c)
	    w[i]=mergevecteur(*w[i]._VECTptr,vecteur(c-ss));
	  else
	    w[i]=vecteur(w[i]._VECTptr->begin(),w[i]._VECTptr->begin()+c);
	}
	if (s<l)
	  w=mergevecteur(w,vecteur(l-s,vecteur(c)));
	else
	  w=vecteur(w.begin(),w.begin()+l);
	return gen(makefreematrice(w),_MATRIX__VECT);
      }
      else {
	vecteur res;
	if (s<l*c)
	  w=mergevecteur(w,vecteur(l*c-s));
	for (int i=0;i<l;++i)
	  res.push_back(vecteur(w.begin()+i*c,w.begin()+(i+1)*c));
	if (transpose)
	  res=mtran(res);
	return gen(makefreematrice(res),_MATRIX__VECT);
      }
    }
    // v[2] as a function, should take 2 args
    gen f=v[2];
    if (!f.is_symb_of_sommet(at_program))
      return gen(vecteur(l,vecteur(c,f)),_MATRIX__VECT);
    vecteur res(l);
    int decal=array_start(contextptr); //(xcas_mode(contextptr)!=0);
    for (int i=decal;i<l+decal;++i){
      vecteur tmp(c);
      for (int j=decal;j<c+decal;++j)
	tmp[j-decal]=f(gen(makevecteur(i,j),_SEQ__VECT),contextptr);
      res[i-decal]=tmp;
    }
    return gen(res,_MATRIX__VECT);
  }
  static const char _matrix_s []="matrix";
  static define_unary_function_eval (__matrix,&_matrix,_matrix_s);
  define_unary_function_ptr5( at_matrix ,alias_at_matrix,&__matrix,0,true);

  static string printasbreak(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (abs_calc_mode(contextptr)==38)
      return "BREAK ";
    if (xcas_mode(contextptr)==3)
      return "Exit ";
    else
      return sommetstr;
  }
  gen _break(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_break,0);
  }
  static const char _break_s []="break";
  static define_unary_function_eval2_index (104,__break,&_break,_break_s,&printasbreak);
  define_unary_function_ptr5( at_break ,alias_at_break ,&__break,0,T_BREAK);

  static string printascontinue(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (abs_calc_mode(contextptr)==38)
      return "CONTINUE ";
    if (xcas_mode(contextptr)==3)
      return "Cycle ";
    else
      return sommetstr;
  }
  gen _continue(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_continue,0);
  }
  static const char _continue_s []="continue";
  static define_unary_function_eval2_index (106,__continue,&_continue,_continue_s,&printascontinue);
  define_unary_function_ptr( at_continue ,alias_at_continue ,&__continue);

  static string printaslabel(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (xcas_mode(contextptr)==3)
      return "Lbl "+feuille.print(contextptr);
    else
      return "label "+feuille.print(contextptr);
  }
  gen _label(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return symbolic(at_label,args);
  }
  static const char _label_s []="label";
  static define_unary_function_eval2 (__label,&_label,_label_s,&printaslabel);
  define_unary_function_ptr5( at_label ,alias_at_label ,&__label,0,T_RETURN);

  static string printasgoto(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (xcas_mode(contextptr)==3)
      return "Goto "+feuille.print(contextptr);
    else
      return "goto "+feuille.print(contextptr);
  }
  gen _goto(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2)
      return _position(change_subtype(args,0),contextptr);
    return symbolic(at_goto,args);
  }
  static const char _goto_s []="goto";
  static define_unary_function_eval2 (__goto,&_goto,_goto_s,&printasgoto);
  define_unary_function_ptr5( at_goto ,alias_at_goto ,&__goto,0,T_RETURN);

  /*
  static vecteur local_vars(const vecteur & v,GIAC_CONTEXT){
    const_iterateur it=v.begin(),itend=v.end();
    vecteur res;
    for (;it!=itend;++it){
      if (it->type==_IDNT && 
	  (contextptr?contextptr->tabptr->find(*it->_IDNTptr->name)==contextptr->tabptr->end():(!it->_IDNTptr->localvalue || it->_IDNTptr->localvalue->empty()))
	  )
	res.push_back(*it);
    }
    return res;
  }
  */
  static string printastilocal(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return gettext("invalid |");
    return string("(")+feuille._VECTptr->front().print(contextptr)+string("|")+feuille._VECTptr->back().print(contextptr)+')';
  }
  gen _tilocal(const gen & args,const context * contextptr){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return symbolic(at_tilocal,args);
    vecteur & v=*args._VECTptr;
    if (v[1].type==_INT_)
      return _bitor(args,contextptr);
    if (is_equal(v.front()))
      return symb_equal(_tilocal(makesequence((v.front()._SYMBptr->feuille)[0],v.back()),contextptr),_tilocal(makesequence((v.front()._SYMBptr->feuille)[1],v.back()),contextptr));
    // find local variables
    vecteur cond(gen2vecteur(v[1]));
    vecteur docond,vars;
    const_iterateur it=cond.begin(),itend=cond.end();
    for (;it!=itend;++it){
      if (it->type!=_SYMB)
	continue;
      unary_function_ptr & u=it->_SYMBptr->sommet;
      gen & g=it->_SYMBptr->feuille;
      if ( (g.type!=_VECT) || (g._VECTptr->empty()) )
	return gensizeerr(contextptr);
      if (u==at_equal || u==at_equal2){
	gen tmp=g._VECTptr->front(),tmp2=g._VECTptr->back();
	if (tmp.type==_IDNT){
	  gen tmp1(eval(tmp,eval_level(contextptr),contextptr));
	  if (tmp1.type==_IDNT)
	    tmp=tmp1;
	  tmp.subtype=0; // otherwise if inside a folder sto will affect tmp!
	  vars.push_back(tmp);
	  tmp2=subst(tmp2,tmp,symb_quote(tmp),false,contextptr);
	}
	docond.push_back(symbolic(at_sto,gen(makevecteur(tmp2,tmp),_SEQ__VECT)));
	continue;
      }
      if (u==at_sto){
	if (g._VECTptr->back().type==_IDNT)
	  vars.push_back(g._VECTptr->back());
	docond.push_back(*it);
	continue;
      }
      gen gf=g._VECTptr->front();
      if (gf.type==_IDNT)
	vars.push_back(gf);
      if (gf.type==_SYMB && gf._SYMBptr->feuille.type==_VECT && gf._SYMBptr->feuille._VECTptr->size()==2 && gf._SYMBptr->feuille._VECTptr->front().type==_IDNT)
	vars.push_back(gf._SYMBptr->feuille._VECTptr->front());
      if (it->type==_SYMB && it->_SYMBptr->sommet!=at_superieur_strict && it->_SYMBptr->sommet!=at_superieur_egal && it->_SYMBptr->sommet!=at_inferieur_strict && it->_SYMBptr->sommet!=at_inferieur_egal &&it->_SYMBptr->sommet!=at_and)
	return gensizeerr(gettext("Invalid |"));
      docond.push_back(symbolic(at_assume,*it));
    }
    vecteur v0(vars.size(),zero);
    gen gv(v[0]);
    // Replace v[0] by its value if it is a global identifier
    if (gv.type==_IDNT){
      if (contextptr){
	sym_tab::const_iterator it=contextptr->tabptr->find(gv._IDNTptr->id_name),itend=contextptr->tabptr->end();
	if (it!=itend)
	  gv=it->second;
      }
      else {
	if (gv._IDNTptr->value)
	  gv=*gv._IDNTptr->value;
      }
    }
    /*
    // Replace local variables by their value in gv
    vecteur vname(*_lname(gv,contextptr)._VECTptr),docondvar(*_lname(docond,contextptr)._VECTptr);
    vecteur vval(vname);
    iterateur jt=vval.begin(),jtend=vval.end();
    for (;jt!=jtend;++jt){
      if (jt->type!=_IDNT || equalposcomp(docondvar,*jt))
	continue;
      if (contextptr){
	sym_tab::const_iterator kt=contextptr->tabptr->find(*jt->_IDNTptr->name);
	if (kt!=contextptr->tabptr->end())
	  *jt=kt->second;
      }
      else {
	if (jt->_IDNTptr->localvalue && !jt->_IDNTptr->localvalue->empty())
	  *jt=jt->_IDNTptr->localvalue->back();
      }
    }
    gv=quotesubst(gv,vname,vval,contextptr);
    */
    // Replace vars global IDNT by local IDNT
    vecteur vname=vars;
    iterateur jt=vname.begin(),jtend=vname.end();
    for (;jt!=jtend;++jt)
      jt->subtype=_GLOBAL__EVAL;
    vecteur vval=vars;
    jt=vval.begin(),jtend=vval.end();
    for (;jt!=jtend;++jt)
      jt->subtype=0;
    gv=quotesubst(gv,vname,vval,contextptr);
    docond=*quotesubst(docond,vname,vval,contextptr)._VECTptr;
    gen prg=symb_program(gen(vname,_SEQ__VECT),gen(v0,_SEQ__VECT),symb_bloc(makevecteur(docond,gv)),contextptr);
    return prg(gen(v0,_SEQ__VECT),contextptr);
  }
  static const char _tilocal_s []="|";
  static define_unary_function_eval4_index (103,__tilocal,&_tilocal,_tilocal_s,&printastilocal,&texprintsommetasoperator);
  define_unary_function_ptr5( at_tilocal ,alias_at_tilocal,&__tilocal,_QUOTE_ARGUMENTS,0);

  static string printasdialog(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return "Dialog "+symbolic(at_bloc,feuille).print(contextptr)+indent(contextptr)+"EndDialog";
  }  
  string printasinputform(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (xcas_mode(contextptr)==3)
      return printasdialog(feuille,sommetstr,contextptr);
    return sommetstr+("("+feuille.print(contextptr)+")");
  }  

  // Eval everything except IDNT and symbolics with
  vecteur inputform_pre_analysis(const gen & g,GIAC_CONTEXT){
    vecteur v(gen2vecteur(g));
    if (python_compat(contextptr) && (v.empty() || v.front()!=at_getKey)){
      gen g_=eval(g,1,contextptr);
      if (g_.type!=_STRNG)
	g_=string2gen(g_.print(contextptr),false);
      v=makevecteur(g_,g_,identificateur("_input_"),1);
      // v=gen2vecteur(eval(g,1,contextptr));
    }
    int s=int(v.size());
    for (int i=0;i<s;++i){
      if (v[i].type==_IDNT || v[i].type!=_SYMB)
	continue;
      unary_function_ptr & u =v[i]._SYMBptr->sommet;
      if ( (u==at_output) || (u==at_Text) || (u==at_Title) || (u==at_click) || (u==at_Request) || (u==at_choosebox) || (u==at_DropDown) || (u==at_Popup) || u==at_of || u==at_at)
	continue;
      v[i]=protecteval(v[i],eval_level(contextptr),contextptr);
    }
    return v;
  }
  gen inputform_post_analysis(const vecteur & v,const gen & res,GIAC_CONTEXT){
    gen tmp=res.eval(eval_level(contextptr),contextptr);
    if (tmp.type==_VECT && !tmp._VECTptr->empty() 
	&& python_compat(contextptr)
	)
      return tmp._VECTptr->back();
    return tmp;
  }
  // user input sent back to the parent process
  gen _inputform(const gen & args,GIAC_CONTEXT){
    if (interactive_op_tab && interactive_op_tab[1])
      return interactive_op_tab[1](args,contextptr);
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    string cs(gettext("inputform may be used in a window environment only"));
#if defined WIN32 || (!defined HAVE_SIGNAL_H_OLD)
    *logptr(contextptr) << cs << '\n';
    return string2gen(cs,false);
#else
    if (child_id){ 
      *logptr(contextptr) << cs << '\n';
      return string2gen(cs,false);
    }
    // pre-analysis
    vecteur v(gen2vecteur(args));
    // int vs=signed(v.size());
    gen res;
    // form
    ofstream child_out(cas_sortie_name().c_str());
    gen e(symbolic(at_inputform,args));
    *logptr(contextptr) << gettext("Archiving ") << e << '\n';
    archive(child_out,e,contextptr);
    archive(child_out,e,contextptr);
    if ( (args.type==_VECT) && (args._VECTptr->empty()) )
      child_out << "User input requested\n" << '¤' ;
    else
      child_out << args << '¤' ;
    child_out.close();
    kill_and_wait_sigusr2();
    ifstream child_in(cas_entree_name().c_str());
    res= unarchive(child_in,contextptr);
    child_in.close();
    *logptr(contextptr) << gettext("Inputform reads ") << res << '\n';
    // post analysis
    return inputform_post_analysis(v,res,contextptr);
#endif
  }
  static const char _inputform_s []="inputform";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval(__inputform,&_inputform,_inputform_s);
#else
  unary_function_eval __inputform(1,&_inputform,_inputform_s,&printasinputform);
#endif
  define_unary_function_ptr5( at_inputform ,alias_at_inputform,&__inputform,_QUOTE_ARGUMENTS,true);

  gen _choosebox(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(symbolic(at_choosebox,args),contextptr);
  }
  static const char _choosebox_s []="choosebox";
  static define_unary_function_eval_quoted (__choosebox,&_choosebox,_choosebox_s);
  define_unary_function_ptr5( at_choosebox ,alias_at_choosebox,&__choosebox,_QUOTE_ARGUMENTS,true);

  gen _output(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(symbolic(at_output,args),contextptr);
  }
  static const char _output_s []="output";
  static define_unary_function_eval_quoted (__output,&_output,_output_s);
  define_unary_function_ptr5( at_output ,alias_at_output,&__output,_QUOTE_ARGUMENTS,true);

  gen _input(const gen & args,bool textinput,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    vecteur v(gen2vecteur(args));
    gen res(args);
    if (python_compat(contextptr)){
      res=eval(args,1,contextptr);
      if (res.type!=_STRNG)
	res=string2gen(res.print(contextptr),false);
#ifdef GIAC_HAS_STO_38
      if (res.type==_STRNG)
	res=string2gen(aspen_input(res._STRNGptr->c_str()),false);
      return res;
#endif
      return __click.op(makevecteur(res,0,identificateur("_input_"),1),contextptr);
    }
    const_iterateur it=v.begin(),itend=v.end();
    if (it==itend)
      return __click.op(args,contextptr);
    if (res.type==_STRNG){
      return __click.op(res,contextptr);
    }
    for (;it!=itend;++it){
      if (it->type==_IDNT || it->is_symb_of_sommet(at_at) || it->is_symb_of_sommet(at_of)){
	if (textinput)
	  res=__click.op(makevecteur(string2gen(it->print(contextptr)),0,*it,1),contextptr);
	else
	  res=__click.op(makevecteur(string2gen(it->print(contextptr),false),0,*it),contextptr);
      }
      if (it+1==itend)
	break;
      if (it->type==_STRNG && ( (it+1)->type==_IDNT || (it+1)->is_symb_of_sommet(at_at) || (it+1)->is_symb_of_sommet(at_of))){
	if (textinput)
	  res=__click.op(makevecteur(*it,0,*(it+1),1),contextptr);
	else
	  res=__click.op(makevecteur(*it,0,*(it+1)),contextptr);
	++it;
      }
    }
    if (is_zero(res))
      return gensizeerr(contextptr);
    return res;
  }

  string printastifunction(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type==_VECT && feuille.subtype==_SEQ__VECT){
      if (feuille._VECTptr->empty())
	return string(sommetstr)+" ";
      else
	return sommetstr+(" ("+feuille.print(contextptr)+')');
    }
    return sommetstr+(" "+feuille.print(contextptr));
  }
  gen _Text(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(symbolic(at_Text,args),contextptr);
  }
  static const char _Text_s []="Text";
  static define_unary_function_eval2_quoted (__Text,&_Text,_Text_s,&printastifunction);
  define_unary_function_ptr5( at_Text ,alias_at_Text,&__Text,_QUOTE_ARGUMENTS,0);

  gen _Title(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(symbolic(at_Title,args),contextptr);
  }
  static const char _Title_s []="Title";
  static define_unary_function_eval2_quoted (__Title,&_Title,_Title_s,&printastifunction);
  define_unary_function_ptr5( at_Title ,alias_at_Title,&__Title,_QUOTE_ARGUMENTS,0);

  gen _Request(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(symbolic(at_Request,args),contextptr);
  }
  static const char _Request_s []="Request";
  static define_unary_function_eval2_quoted (__Request,&_Request,_Request_s,&printastifunction);
  define_unary_function_ptr5( at_Request ,alias_at_Request,&__Request,_QUOTE_ARGUMENTS,0);

  gen _DropDown(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(symbolic(at_DropDown,args),contextptr);
  }
  static const char _DropDown_s []="DropDown";
  static define_unary_function_eval2_quoted (__DropDown,&_DropDown,_DropDown_s,&printastifunction);
  define_unary_function_ptr5( at_DropDown ,alias_at_DropDown,&__DropDown,_QUOTE_ARGUMENTS,0);

  gen _Popup(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(symbolic(at_Popup,args),contextptr);
  }
  static const char _Popup_s []="Popup";
  static define_unary_function_eval2_quoted (__Popup,&_Popup,_Popup_s,&printastifunction);
  define_unary_function_ptr5( at_Popup ,alias_at_Popup,&__Popup,_QUOTE_ARGUMENTS,0);

  gen _Dialog(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    return __inputform.op(args,contextptr);
  }
  static const char _Dialog_s []="Dialog";
  static define_unary_function_eval2_index (89,__Dialog,&_Dialog,_Dialog_s,&printasdialog);
  define_unary_function_ptr5( at_Dialog ,alias_at_Dialog,&__Dialog,_QUOTE_ARGUMENTS,0);

  gen _expr(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_STRNG && args._VECTptr->back().type==_INT_){
      int mode=args._VECTptr->back().val;
      bool rpnmode=mode<0;
      mode=absint(mode) % 256;
      if (mode>3)
	return gensizeerr(contextptr);
      int save_mode=xcas_mode(contextptr);
      bool save_rpnmode=rpn_mode(contextptr);
      xcas_mode(contextptr)=mode;
      rpn_mode(contextptr)=rpnmode;
      gen res=eval(gen(*args._VECTptr->front()._STRNGptr,contextptr),eval_level(contextptr),contextptr);
      xcas_mode(contextptr)=save_mode;
      rpn_mode(contextptr)=save_rpnmode;
      return res;
    }
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_STRNG && args._VECTptr->back()==at_quote)
      return gen(*args._VECTptr->front()._STRNGptr,contextptr);
    if (args.type==_VECT && !args._VECTptr->empty() && args._VECTptr->front().type==_FUNC){
      vecteur v(args._VECTptr->begin()+1,args._VECTptr->end());
      return symbolic(*args._VECTptr->front()._FUNCptr,gen(v,_SEQ__VECT));
    }
    if (args.type!=_STRNG)
      return symbolic(at_expr,args);
    gen g(*args._STRNGptr,contextptr);
    if (giac::first_error_line(contextptr))
      return gensizeerr(string(gettext("Syntax compatibility mode "))+print_program_syntax(xcas_mode(contextptr))+gettext(". Parse error line ")+print_INT_(giac::first_error_line(contextptr)) + gettext(" column ")+print_INT_(giac::lexer_column_number(contextptr))+  gettext(" at ")  + giac::error_token_name(contextptr)) ;
    return eval(g,eval_level(contextptr),contextptr);
  }
  static const char _expr_s []="expr";
  static define_unary_function_eval (__expr,&_expr,_expr_s);
  define_unary_function_ptr5( at_expr ,alias_at_expr,&__expr,0,true);

  static const char _execute_s []="execute";
  static define_unary_function_eval (__execute,&_expr,_execute_s);
  define_unary_function_ptr5( at_execute ,alias_at_execute,&__execute,0,true);

  gen _string(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    int maxp=MAX_PRINTABLE_ZINT;
    MAX_PRINTABLE_ZINT=1000000;
    string res;
    if (args.type==_VECT && args.subtype==_SEQ__VECT){
      const_iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
      for (;it!=itend;){
	if (it->type!=_STRNG){
	  res += it->print(contextptr);
	  ++it;
	  if (it!=itend)
	    res += ','; 
	  continue;
	}
	res += *it->_STRNGptr;
	++it;
	if (it==itend)
	  return string2gen(res,false);
	if (it->type==_STRNG)
	  res += '\n';
      }
    }
    else
      res=args.print(contextptr);
    MAX_PRINTABLE_ZINT=maxp;
    return string2gen(res,false);
  }
  static const char _string_s []="string";
  static define_unary_function_eval (__string,&_string,_string_s);
  define_unary_function_ptr5( at_string ,alias_at_string,&__string,0,true);

  static const char _str_s []="str";
  static define_unary_function_eval (__str,&_string,_str_s);
  define_unary_function_ptr5( at_str ,alias_at_str,&__str,0,true);

  gen _part(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if ( (args.type==_VECT) && args._VECTptr->size()==2 ){
      gen & i=args._VECTptr->back();
      gen & g=args._VECTptr->front();
      if (i.type!=_INT_ || i.val<=0){
	if (g.type!=_SYMB)
	  return string2gen(g.print(contextptr),false);
	else
	  return string2gen(g._SYMBptr->sommet.ptr()->s,false);
      }
      else {
	if (g.type!=_SYMB){
	  if (i.val!=1)
	    return gensizeerr(contextptr);
	  return g;
	}
	else {
	  vecteur v(gen2vecteur(g._SYMBptr->feuille));
	  if (signed(v.size())<i.val)
	    return gensizeerr(contextptr);
	  return v[i.val-1];
	}
      }
    }
    if (args.type==_SYMB)
      return int(gen2vecteur(args._SYMBptr->feuille).size());
    return 0;
  }
  static const char _part_s []="part";
  static define_unary_function_eval (__part,&_part,_part_s);
  define_unary_function_ptr5( at_part ,alias_at_part,&__part,0,true);

  string tiasc_translate(const string & s){
    int l=int(s.size());
    string t("");
    for (int i=0;i<l;++i){
      char c=s[i];
      if (c=='\r')
	continue;
      if (c=='@'){
	t += "//";
	continue;
      }
      if (c=='\\'){
	++i;
	string ti_escape("");
	for (;i<l;++i){
	  char c=s[i];
	  if (c=='\\' || c==' '){
	    break;
	  }
	  ti_escape += c;
	}
	if (i==l || c==' ')
	  return t+"::"+ti_escape;
	if (ti_escape=="->"){
	  t += "=>";
	  continue;
	}
	if (ti_escape=="(C)"){ // comment
	  t += "//";
	  continue;
	}
	if (ti_escape=="(-)"){
	  t += '-';
	  continue;
	}
	if (ti_escape=="e"){
	  t += "exp(1)";
	  continue;
	}
	if (ti_escape=="i"){
	  t += '\xa1';
	  continue;
	}
	t += ti_escape;
      }
      else
	t += c;
    }
    if (t==string(t.size(),' '))
      return "";
    return t;
  }

  gen _Pause(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen g1=g;
    if (is_integer(g1) || g1.type==_REAL)
      g1=evalf_double(g,1,contextptr);
    if (g1.type==_DOUBLE_){
      wait_1ms(g1._DOUBLE_val*1000);
    }
    else
      __interactive.op(symbolic(at_Pause,g),contextptr);
    return 0;
  }
  static const char _Pause_s []="Pause";
  static define_unary_function_eval2 (__Pause,&_Pause,_Pause_s,&printastifunction);
  define_unary_function_ptr5( at_Pause ,alias_at_Pause ,&__Pause,0,T_RETURN);

  static const char _sleep_s []="sleep";
  static define_unary_function_eval (__sleep,&_Pause,_sleep_s);
  define_unary_function_ptr5( at_sleep ,alias_at_sleep,&__sleep,0,true);

  gen _monotonic(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
#ifdef NSPIRE_NEWLIB
    return int(unsigned(millis()/1000.) & ((1u<<31)-1));
#endif
#ifdef FXCG
    return RTC_GetTicks();
#else
#if defined __APPLE__ || defined EMCC || defined EMCC2 || !defined HAVE_LIBRT
    return (int) clock();
#else
    timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec*1e-9;
#endif
#endif
  }
  static const char _monotonic_s []="monotonic";
  static define_unary_function_eval (__monotonic,&_monotonic,_monotonic_s);
  define_unary_function_ptr5( at_monotonic ,alias_at_monotonic,&__monotonic,0,true);

  static const char _DelVar_s []="DelVar";
  static define_unary_function_eval2_quoted (__DelVar,&_purge,_DelVar_s,&printastifunction);
  define_unary_function_ptr5( at_DelVar ,alias_at_DelVar,&__DelVar,_QUOTE_ARGUMENTS,T_RETURN);

  static const char _del_s []="del";
  static define_unary_function_eval2_quoted (__del,&_purge,_del_s,&printastifunction);
  define_unary_function_ptr5( at_del ,alias_at_del,&__del,_QUOTE_ARGUMENTS,T_RETURN);

  gen _Row(const gen & g,GIAC_CONTEXT){
    if (interactive_op_tab && interactive_op_tab[6])
      return interactive_op_tab[6](g,contextptr);
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    return spread_Row(contextptr);
  }
  static const char _Row_s []="Row";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval(__Row,&_Row,_Row_s);
#else
  unary_function_eval __Row(0,&_Row,_Row_s,&printastifunction);
#endif
  define_unary_function_ptr( at_Row ,alias_at_Row ,&__Row);

  gen _Col(const gen & g,GIAC_CONTEXT){
    if (interactive_op_tab && interactive_op_tab[7])
      return interactive_op_tab[7](g,contextptr);
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    return spread_Col(contextptr);
  }
  static const char _Col_s []="Col";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval(__Col,&_Col,_Col_s);
#else
  unary_function_eval __Col(0,&_Col,_Col_s,&printastifunction);
#endif
  define_unary_function_ptr( at_Col ,alias_at_Col ,&__Col);

  gen matrix_apply(const gen & a,const gen & b,gen (* f) (const gen &, const gen &) ){
    if (a.type!=_VECT || b.type!=_VECT || a._VECTptr->size()!=b._VECTptr->size())
      return apply(a,b,f);
    const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end(),jt=b._VECTptr->begin();
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it,++jt){
      res.push_back(apply(*it,*jt,f));
    }
    return gen(res,a.subtype);
  }
  gen matrix_apply(const gen & a,const gen & b,GIAC_CONTEXT,gen (* f) (const gen &, const gen &,GIAC_CONTEXT) ){
    if (a.type!=_VECT || b.type!=_VECT || a._VECTptr->size()!=b._VECTptr->size())
      return apply(a,b,contextptr,f);
    const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end(),jt=b._VECTptr->begin();
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it,++jt){
      res.push_back(apply(*it,*jt,contextptr,f));
    }
    return gen(res,a.subtype);
  }
  gen prod(const gen & a,const gen &b){
    return a*b;
  }
  gen somme(const gen & a,const gen &b){
    return a+b;
  }
  gen _pointprod(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen a,b;
    if (!check_binary(g,a,b))
      return a;
    return matrix_apply(a,b,contextptr,operator_times);
  }
  static const char _pointprod_s []=".*";
  static define_unary_function_eval4_index (92,__pointprod,&_pointprod,_pointprod_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_pointprod ,alias_at_pointprod ,&__pointprod);

  gen pointdivision(const gen & a,const gen &b,GIAC_CONTEXT){
    if (a.type==_VECT && b.type!=_VECT)
      return apply1st(a,b,contextptr,pointdivision);
    if (a.type!=_VECT && b.type==_VECT)
      return apply2nd(a,b,contextptr,pointdivision);
    //return a/b;
    // reactivated 22 oct 15 for [[1,2],[3,4]] ./ [[3,4],[5,6]]
    return matrix_apply(a,b,contextptr,rdiv);
  }
  gen _pointdivision(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen a,b;
    if (!check_binary(g,a,b))
      return a;
    return pointdivision(a,b,contextptr);
  }
  static const char _pointdivision_s []="./";
  static define_unary_function_eval4_index (94,__pointdivision,&_pointdivision,_pointdivision_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_pointdivision ,alias_at_pointdivision ,&__pointdivision);

  gen giac_pow(const gen &,const gen &,GIAC_CONTEXT);
  gen pointpow(const gen & a,const gen &b,GIAC_CONTEXT){
    if (b.type!=_VECT && a.type==_VECT){
      return apply(a,b,contextptr,pointpow);
    }
    return matrix_apply(a,b,contextptr,giac_pow);
  }
  gen _pointpow(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    gen a,b;
    if (!check_binary(g,a,b))
      return a;
    return pointpow(a,b,contextptr);
  }
  static const char _pointpow_s []=".^";
  static define_unary_function_eval4_index (96,__pointpow,&_pointpow,_pointpow_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_pointpow ,alias_at_pointpow ,&__pointpow);

  string printassuffix(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return feuille.print(contextptr)+sommetstr;
  }  
  gen _pourcent(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    return rdiv(g,100,contextptr);
  }
  static const char _pourcent_s []="%";
  static define_unary_function_eval2_index (100,__pourcent,&_pourcent,_pourcent_s,&printassuffix);
  define_unary_function_ptr( at_pourcent ,alias_at_pourcent ,&__pourcent);

  gen _hash(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (g.type!=_STRNG)
      return g;
    return gen(*g._STRNGptr,contextptr);
  }
  static const char _hash_s []="make_symbol";
  static define_unary_function_eval_index (98,__hash,&_hash,_hash_s);
  define_unary_function_ptr5( at_hash ,alias_at_hash ,&__hash,0,true);

  bool user_screen=false;
  int user_screen_io_x=0,user_screen_io_y=0;
  int user_screen_fontsize=14;
  gen _interactive(const gen & args,GIAC_CONTEXT){
    if (interactive_op_tab && interactive_op_tab[2])
      return interactive_op_tab[2](args,contextptr);
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
#if defined(WIN32) || !defined(HAVE_SIGNAL_H_OLD)
    return 0;
#else
    if (child_id)
      return 0;
    ofstream child_out(cas_sortie_name().c_str());
    gen e(symbolic(at_interactive,args));
    // *logptr(contextptr) << e << '\n';
    archive(child_out,e,contextptr);
    archive(child_out,e,contextptr);
    child_out.close();
    kill_and_wait_sigusr2();
    ifstream child_in(cas_entree_name().c_str());
    gen res= unarchive(child_in,contextptr);
    child_in.close();
    return res;
#endif
  }
  static const char _interactive_s []="interactive";
#if defined RTOS_THREADX || defined NSPIRE || defined FXCG
  define_unary_function_eval_index(1,__interactive,&_interactive,_interactive_s);
  // const unary_function_eval __interactive(1,&_interactive,_interactive_s);
#else
  unary_function_eval __interactive(1,&_interactive,_interactive_s);
#endif
  define_unary_function_ptr5( at_interactive ,alias_at_interactive,&__interactive,_QUOTE_ARGUMENTS,true);

  // v=[ [idnt,value] ... ]
  // search g in v if found return value
  // else return g unevaluated
  gen find_in_folder(vecteur & v,const gen & g){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (it->type!=_VECT || it->_VECTptr->size()!=2)
	continue;
      vecteur & w=*it->_VECTptr;
      if (w[0]==g)
	return w[1];
    }
    return g;
  }

  gen _ti_semi(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG &&  args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return symbolic(at_ti_semi,args);
    vecteur & v=*args._VECTptr;
    matrice m1,m2;
    if (!ckmatrix(v[0])){
      if (v[0].type==_VECT)
	m1=vecteur(1,*v[0]._VECTptr);
      else
	m1=vecteur(1,vecteur(1,v[0]));
    }
    else
      m1=*v[0]._VECTptr;
    if (!ckmatrix(v[1])){
      if (v[1].type==_VECT)
	m2=vecteur(1,*v[1]._VECTptr);
      else
	m2=vecteur(1,vecteur(1,v[1]));
    }
    else
      m2=*v[1]._VECTptr;
    // *logptr(contextptr) << m1 << " " << m2 << '\n';
    return mergevecteur(m1,m2); 
  }
  static const char _ti_semi_s []=";";
  static define_unary_function_eval4 (__ti_semi,&_ti_semi,_ti_semi_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_ti_semi ,alias_at_ti_semi ,&__ti_semi);

  gen widget_size(const gen & g,GIAC_CONTEXT){
    if (interactive_op_tab && interactive_op_tab[9])
      return interactive_op_tab[9](g,contextptr);
    return zero;
  }
  static const char _widget_size_s []="widget_size";
  // const string _widget_size_s ="widget_size";
#if defined RTOS_THREADX || defined NSPIRE
  define_unary_function_eval(__widget_size,&widget_size,_widget_size_s);
  // const unary_function_eval __widget_size(0,&widget_size,_widget_size_s);
#else
  unary_function_eval __widget_size(0,&widget_size,_widget_size_s);
#endif
  define_unary_function_ptr5( at_widget_size ,alias_at_widget_size,&__widget_size,0,true);

  gen keyboard(const gen & g,GIAC_CONTEXT){
    return zero;
  }
  static const char _keyboard_s []="keyboard";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval(__keyboard,&keyboard,_keyboard_s);
  // const unary_function_eval __keyboard(0,&keyboard,_keyboard_s);
#else
  unary_function_eval __keyboard(0,&keyboard,_keyboard_s);
#endif
  define_unary_function_ptr5( at_keyboard ,alias_at_keyboard,&__keyboard,0,true);

#if !defined KHICAS && !defined SDL_KHICAS // see kadd.cc
  gen current_sheet(const gen & g,GIAC_CONTEXT){
#if (defined EMCC || defined EMCC2 ) && !defined GIAC_GGB
    if (ckmatrix(g,true)){
      matrice m=*g._VECTptr;
      int R=m.size(),C=m.front()._VECTptr->size();
      CERR << "current1 " << R << " " << C << '\n';
      R=giacmin(R,EM_ASM_INT({ return UI.assistant_matr_maxrows; },0));
      C=giacmin(C,EM_ASM_INT({ return UI.assistant_matr_maxcols; },0));
      CERR << "current2 " << R << " " << C << '\n';
      int save_r=printcell_current_row(contextptr);
      int save_c=printcell_current_col(contextptr);
      for (int i=0;i<R;++i){
	printcell_current_row(contextptr)=i;
	for (int j=0;j<C;++j){
	  printcell_current_col(contextptr)=j;
	  string s=m[i][j].print(contextptr);
	  CERR << "current3 " << s << " " << i << " " << j << '\n';
	  EM_ASM_ARGS({
	      var s=UTF8ToString($0);//Pointer_stringify($0);//
	      console.log(s);
	      UI.sheet_set_ij(s,$1,$2);
	    },s.c_str(),i,j);
	}
      }
      printcell_current_col(contextptr)=save_c;
      printcell_current_row(contextptr)=save_r;
      EM_ASM_ARGS({var s=' ';UI.sheet_recompute(s.substr(0,0)); UI.open_sheet(true);},0);
    }
#endif
    if (interactive_op_tab && interactive_op_tab[5])
      return interactive_op_tab[5](g,contextptr);
    return zero;
  }
  static const char _current_sheet_s []="current_sheet";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval(__current_sheet,&current_sheet,_current_sheet_s);
#else
  unary_function_eval __current_sheet(1,&current_sheet,_current_sheet_s);
#endif
  define_unary_function_ptr5( at_current_sheet ,alias_at_current_sheet,&__current_sheet,_QUOTE_ARGUMENTS,true);
#endif
  
  static string printasmaple_lib(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_VECT || feuille._VECTptr->size()!=2)
      return gettext("Error printasmaple_lib");
    vecteur & v=*feuille._VECTptr;
    return v[0].print(contextptr)+"["+v[1].print(contextptr)+"]";
  }
  gen maple_lib(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    vecteur & v=*g._VECTptr;
    return v[1];
  }
  static const char _maple_lib_s[]="maple_lib";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval2_index(110,__maple_lib,&maple_lib,_maple_lib_s,&printasmaple_lib);
#else
  const unary_function_eval __maple_lib(110,&maple_lib,_maple_lib_s,&printasmaple_lib);
#endif
  define_unary_function_ptr( at_maple_lib ,alias_at_maple_lib ,&__maple_lib);

  gen window_switch(const gen & g,GIAC_CONTEXT){
    return zero; // defined by GUI handler
  }
  static const char _window_switch_s []="window_switch";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval(__window_switch,&window_switch,_window_switch_s);
#else
  const unary_function_eval __window_switch(0,&window_switch,_window_switch_s);
#endif
  define_unary_function_ptr( at_window_switch ,alias_at_window_switch ,&__window_switch);


  // Funcion has been changed -> simplify
  static const char _simplifier_s []="simplifier";
  static define_unary_function_eval (__simplifier,&_simplify,_simplifier_s);
  define_unary_function_ptr5( at_simplifier ,alias_at_simplifier,&__simplifier,0,true);

  gen _regroup(const gen &g,GIAC_CONTEXT){
    return _simplifier(g,contextptr);
  }
  static const char _regrouper_s []="regroup";
  static define_unary_function_eval (__regrouper,&_regroup,_regrouper_s);
  define_unary_function_ptr5( at_regrouper ,alias_at_regrouper,&__regrouper,0,true);

  gen find_or_make_symbol(const string & s,bool check38,GIAC_CONTEXT){
    gen tmp;
    find_or_make_symbol(s,tmp,0,check38,contextptr);
    return tmp;
  }

  // fundemental metric units
  const mksa_unit __m_unit={1,1,0,0,0,0,0,0,0};
  const mksa_unit __kg_unit={1,0,1,0,0,0,0,0,0};
  const mksa_unit __s_unit={1,0,0,1,0,0,0,0,0};
  const mksa_unit __A_unit={1,0,0,0,1,0,0,0,0};
  const mksa_unit __K_unit={1,0,0,0,0,1,0,0,0};
  const mksa_unit __mol_unit={1,0,0,0,0,0,1,0,0};
  const mksa_unit __cd_unit={1,0,0,0,0,0,0,1,0};
  const mksa_unit __E_unit={1,0,0,0,0,0,0,0,1};
  const mksa_unit __Bq_unit={1,0,0,-1,0,0,0,0,0};
  const mksa_unit __C_unit={1,0,0,1,1,0,0,0,0};
  const mksa_unit __F_unit={1,-2,-1,4,2,0,0,0,0};
  const mksa_unit __Gy_unit={1,2,0,-2,0,0,0,0,0};
  const mksa_unit __H_unit={1,2,1,-2,-2,0,0,0,0};
  const mksa_unit __Hz_unit={1,0,0,-1,0,0,0,0,0};
  const mksa_unit __J_unit={1,2,1,-2,0,0,0,0,0};
  const mksa_unit __mho_unit={1,-2,-1,3,2,0,0,0,0};
  const mksa_unit __N_unit={1,1,1,-2,0,0,0,0,0};
  const mksa_unit __Ohm_unit={1,2,1,-3,-2,0,0,0,0};
  const mksa_unit __Pa_unit={1,-1,1,-2,0,0,0,0,0};
  const mksa_unit __rad_unit={1,0,0,0,0,0,0,0,0};
  const mksa_unit __S_unit={1,-2,-1,3,2,0,0,0,0};
  const mksa_unit __Sv_unit={1,2,0,-2,0,0,0,0,0};
  const mksa_unit __T_unit={1,0,1,-2,-1,0,0,0,0};
  const mksa_unit __V_unit={1,2,1,-3,-1,0,0,0,0};
  const mksa_unit __W_unit={1,2,1,-3,0,0,0,0,0};
  const mksa_unit __Wb_unit={1,2,1,-2,-1,0,0,0,0};
  const mksa_unit __Js_unit={1,2,1,-1,0,0,0,0,0};
  // To each unit we associate a number and a vector of powers of kg, m, s
  /*
  map_charptr_gen unit_conversion_map;
  gen mksa_register(const char * s,const gen & equiv){
    unit_conversion_map[s+1]=equiv;
    return find_or_make_symbol(s,false,context0);
  }
  */
#ifdef USTL
  ustl::map<const char *, const mksa_unit *,ltstr> & unit_conversion_map(){
    static ustl::map<const char *, const mksa_unit *,ltstr> * unit_conversion_mapptr=0;
    if (!unit_conversion_mapptr)
      unit_conversion_mapptr=new ustl::map<const char *, const mksa_unit *,ltstr>;
    return *unit_conversion_mapptr;
  }
#else
  std::map<const char *, const mksa_unit *,ltstr> & unit_conversion_map(){
    static std::map<const char *, const mksa_unit *,ltstr> * unit_conversion_mapptr=0;
    if (!unit_conversion_mapptr)
      unit_conversion_mapptr=new std::map<const char *, const mksa_unit *,ltstr>;
    return *unit_conversion_mapptr;
  }
#endif

#ifdef STATIC_BUILTIN_LEXER_FUNCTIONS
  static const alias_identificateur alias_identificateur_A_unit_rom={0,0,"_A",0,0};
  const identificateur & _IDNT_id_A_unit_rom=* (const identificateur *) &alias_identificateur_A_unit_rom;
  const alias_ref_identificateur ref_A_unit_rom={-1,0,0,"_A",0,0};
  const define_alias_gen(alias_A_unit_rom,_IDNT,0,&ref_A_unit_rom);
  const gen & A_unit_rom_IDNT_e = * (gen *) & alias_A_unit_rom;

  static const alias_identificateur alias_identificateur_Angstrom_unit_rom={0,0,"_Angstrom",0,0};
  const identificateur & _IDNT_id_Angstrom_unit_rom=* (const identificateur *) &alias_identificateur_Angstrom_unit_rom;
  const alias_ref_identificateur ref_Angstrom_unit_rom={-1,0,0,"_Angstrom",0,0};
  const define_alias_gen(alias_Angstrom_unit_rom,_IDNT,0,&ref_Angstrom_unit_rom);
  const gen & Angstrom_unit_rom_IDNT_e = * (gen *) & alias_Angstrom_unit_rom;

  static const alias_identificateur alias_identificateur_Bq_unit_rom={0,0,"_Bq",0,0};
  const identificateur & _IDNT_id_Bq_unit_rom=* (const identificateur *) &alias_identificateur_Bq_unit_rom;
  const alias_ref_identificateur ref_Bq_unit_rom={-1,0,0,"_Bq",0,0};
  const define_alias_gen(alias_Bq_unit_rom,_IDNT,0,&ref_Bq_unit_rom);
  const gen & Bq_unit_rom_IDNT_e = * (gen *) & alias_Bq_unit_rom;

  static const alias_identificateur alias_identificateur_yd_unit_rom={0,0,"_yd",0,0};
  const identificateur & _IDNT_id_yd_unit_rom=* (const identificateur *) &alias_identificateur_yd_unit_rom;
  const alias_ref_identificateur ref_yd_unit_rom={-1,0,0,"_yd",0,0};
  const define_alias_gen(alias_yd_unit_rom,_IDNT,0,&ref_yd_unit_rom);
  const gen & yd_unit_rom_IDNT_e = * (gen *) & alias_yd_unit_rom;

  static const alias_identificateur alias_identificateur_yr_unit_rom={0,0,"_yr",0,0};
  const identificateur & _IDNT_id_yr_unit_rom=* (const identificateur *) &alias_identificateur_yr_unit_rom;
  const alias_ref_identificateur ref_yr_unit_rom={-1,0,0,"_yr",0,0};
  const define_alias_gen(alias_yr_unit_rom,_IDNT,0,&ref_yr_unit_rom);
  const gen & yr_unit_rom_IDNT_e = * (gen *) & alias_yr_unit_rom;

  static const alias_identificateur alias_identificateur_Btu_unit_rom={0,0,"_Btu",0,0};
  const identificateur & _IDNT_id_Btu_unit_rom=* (const identificateur *) &alias_identificateur_Btu_unit_rom;
  const alias_ref_identificateur ref_Btu_unit_rom={-1,0,0,"_Btu",0,0};
  const define_alias_gen(alias_Btu_unit_rom,_IDNT,0,&ref_Btu_unit_rom);
  const gen & Btu_unit_rom_IDNT_e = * (gen *) & alias_Btu_unit_rom;

  static const alias_identificateur alias_identificateur_R__unit_rom={0,0,"_R_",0,0};
  const identificateur & _IDNT_id_R__unit_rom=* (const identificateur *) &alias_identificateur_R__unit_rom;
  const alias_ref_identificateur ref_R__unit_rom={-1,0,0,"_R_",0,0};
  const define_alias_gen(alias_R__unit_rom,_IDNT,0,&ref_R__unit_rom);
  const gen & R__unit_rom_IDNT_e = * (gen *) & alias_R__unit_rom;

  static const alias_identificateur alias_identificateur_epsilon0__unit_rom={0,0,"_epsilon0_",0,0};
  const identificateur & _IDNT_id_epsilon0__unit_rom=* (const identificateur *) &alias_identificateur_epsilon0__unit_rom;
  const alias_ref_identificateur ref_epsilon0__unit_rom={-1,0,0,"_epsilon0_",0,0};
  const define_alias_gen(alias_epsilon0__unit_rom,_IDNT,0,&ref_epsilon0__unit_rom);
  const gen & epsilon0__unit_rom_IDNT_e = * (gen *) & alias_epsilon0__unit_rom;

  static const alias_identificateur alias_identificateur_a0__unit_rom={0,0,"_a0_",0,0};
  const identificateur & _IDNT_id_a0__unit_rom=* (const identificateur *) &alias_identificateur_a0__unit_rom;
  const alias_ref_identificateur ref_a0__unit_rom={-1,0,0,"_a0_",0,0};
  const define_alias_gen(alias_a0__unit_rom,_IDNT,0,&ref_a0__unit_rom);
  const gen & a0__unit_rom_IDNT_e = * (gen *) & alias_a0__unit_rom;

  static const alias_identificateur alias_identificateur_RSun__unit_rom={0,0,"_RSun_",0,0};
  const identificateur & _IDNT_id_RSun__unit_rom=* (const identificateur *) &alias_identificateur_RSun__unit_rom;
  const alias_ref_identificateur ref_RSun__unit_rom={-1,0,0,"_RSun_",0,0};
  const define_alias_gen(alias_RSun__unit_rom,_IDNT,0,&ref_RSun__unit_rom);
  const gen & RSun__unit_rom_IDNT_e = * (gen *) & alias_RSun__unit_rom;

  static const alias_identificateur alias_identificateur_REarth__unit_rom={0,0,"_REarth_",0,0};
  const identificateur & _IDNT_id_REarth__unit_rom=* (const identificateur *) &alias_identificateur_REarth__unit_rom;
  const alias_ref_identificateur ref_REarth__unit_rom={-1,0,0,"_REarth_",0,0};
  const define_alias_gen(alias_REarth__unit_rom,_IDNT,0,&ref_REarth__unit_rom);
  const gen & REarth__unit_rom_IDNT_e = * (gen *) & alias_REarth__unit_rom;

  static const alias_identificateur alias_identificateur_Fdy_unit_rom={0,0,"_Fdy",0,0};
  const identificateur & _IDNT_id_Fdy_unit_rom=* (const identificateur *) &alias_identificateur_Fdy_unit_rom;
  const alias_ref_identificateur ref_Fdy_unit_rom={-1,0,0,"_Fdy",0,0};
  const define_alias_gen(alias_Fdy_unit_rom,_IDNT,0,&ref_Fdy_unit_rom);
  const gen & Fdy_unit_rom_IDNT_e = * (gen *) & alias_Fdy_unit_rom;

  static const alias_identificateur alias_identificateur_F_unit_rom={0,0,"_F",0,0};
  const identificateur & _IDNT_id_F_unit_rom=* (const identificateur *) &alias_identificateur_F_unit_rom;
  const alias_ref_identificateur ref_F_unit_rom={-1,0,0,"_F",0,0};
  const define_alias_gen(alias_F_unit_rom,_IDNT,0,&ref_F_unit_rom);
  const gen & F_unit_rom_IDNT_e = * (gen *) & alias_F_unit_rom;

  static const alias_identificateur alias_identificateur_FF_unit_rom={0,0,"_FF",0,0};
  const identificateur & _IDNT_id_FF_unit_rom=* (const identificateur *) &alias_identificateur_FF_unit_rom;
  const alias_ref_identificateur ref_FF_unit_rom={-1,0,0,"_FF",0,0};
  const define_alias_gen(alias_FF_unit_rom,_IDNT,0,&ref_FF_unit_rom);
  const gen & FF_unit_rom_IDNT_e = * (gen *) & alias_FF_unit_rom;

  static const alias_identificateur alias_identificateur_Faraday__unit_rom={0,0,"_F_",0,0};
  const identificateur & _IDNT_id_Faraday__unit_rom=* (const identificateur *) &alias_identificateur_Faraday__unit_rom;
  const alias_ref_identificateur ref_Faraday__unit_rom={-1,0,0,"_F_",0,0};
  const define_alias_gen(alias_Faraday__unit_rom,_IDNT,0,&ref_Faraday__unit_rom);
  const gen & Faraday__unit_rom_IDNT_e = * (gen *) & alias_Faraday__unit_rom;

  static const alias_identificateur alias_identificateur_G__unit_rom={0,0,"_G_",0,0};
  const identificateur & _IDNT_id_G__unit_rom=* (const identificateur *) &alias_identificateur_G__unit_rom;
  const alias_ref_identificateur ref_G__unit_rom={-1,0,0,"_G_",0,0};
  const define_alias_gen(alias_G__unit_rom,_IDNT,0,&ref_G__unit_rom);
  const gen & G__unit_rom_IDNT_e = * (gen *) & alias_G__unit_rom;

  static const alias_identificateur alias_identificateur_Gal_unit_rom={0,0,"_Gal",0,0};
  const identificateur & _IDNT_id_Gal_unit_rom=* (const identificateur *) &alias_identificateur_Gal_unit_rom;
  const alias_ref_identificateur ref_Gal_unit_rom={-1,0,0,"_Gal",0,0};
  const define_alias_gen(alias_Gal_unit_rom,_IDNT,0,&ref_Gal_unit_rom);
  const gen & Gal_unit_rom_IDNT_e = * (gen *) & alias_Gal_unit_rom;

  static const alias_identificateur alias_identificateur_Gy_unit_rom={0,0,"_Gy",0,0};
  const identificateur & _IDNT_id_Gy_unit_rom=* (const identificateur *) &alias_identificateur_Gy_unit_rom;
  const alias_ref_identificateur ref_Gy_unit_rom={-1,0,0,"_Gy",0,0};
  const define_alias_gen(alias_Gy_unit_rom,_IDNT,0,&ref_Gy_unit_rom);
  const gen & Gy_unit_rom_IDNT_e = * (gen *) & alias_Gy_unit_rom;

  static const alias_identificateur alias_identificateur_H_unit_rom={0,0,"_H",0,0};
  const identificateur & _IDNT_id_H_unit_rom=* (const identificateur *) &alias_identificateur_H_unit_rom;
  const alias_ref_identificateur ref_H_unit_rom={-1,0,0,"_H",0,0};
  const define_alias_gen(alias_H_unit_rom,_IDNT,0,&ref_H_unit_rom);
  const gen & H_unit_rom_IDNT_e = * (gen *) & alias_H_unit_rom;

  static const alias_identificateur alias_identificateur_HFCC_unit_rom={0,0,"_HFCC",0,0};
  const identificateur & _IDNT_id_HFCC_unit_rom=* (const identificateur *) &alias_identificateur_HFCC_unit_rom;
  const alias_ref_identificateur ref_HFCC_unit_rom={-1,0,0,"_HFCC",0,0};
  const define_alias_gen(alias_HFCC_unit_rom,_IDNT,0,&ref_HFCC_unit_rom);
  const gen & HFCC_unit_rom_IDNT_e = * (gen *) & alias_HFCC_unit_rom;

  static const alias_identificateur alias_identificateur_Hz_unit_rom={0,0,"_Hz",0,0};
  const identificateur & _IDNT_id_Hz_unit_rom=* (const identificateur *) &alias_identificateur_Hz_unit_rom;
  const alias_ref_identificateur ref_Hz_unit_rom={-1,0,0,"_Hz",0,0};
  const define_alias_gen(alias_Hz_unit_rom,_IDNT,0,&ref_Hz_unit_rom);
  const gen & Hz_unit_rom_IDNT_e = * (gen *) & alias_Hz_unit_rom;

  static const alias_identificateur alias_identificateur_IO__unit_rom={0,0,"_IO_",0,0};
  const identificateur & _IDNT_id_IO__unit_rom=* (const identificateur *) &alias_identificateur_IO__unit_rom;
  const alias_ref_identificateur ref_IO__unit_rom={-1,0,0,"_IO_",0,0};
  const define_alias_gen(alias_IO__unit_rom,_IDNT,0,&ref_IO__unit_rom);
  const gen & IO__unit_rom_IDNT_e = * (gen *) & alias_IO__unit_rom;

  static const alias_identificateur alias_identificateur_J_unit_rom={0,0,"_J",0,0};
  const identificateur & _IDNT_id_J_unit_rom=* (const identificateur *) &alias_identificateur_J_unit_rom;
  const alias_ref_identificateur ref_J_unit_rom={-1,0,0,"_J",0,0};
  const define_alias_gen(alias_J_unit_rom,_IDNT,0,&ref_J_unit_rom);
  const gen & J_unit_rom_IDNT_e = * (gen *) & alias_J_unit_rom;

  static const alias_identificateur alias_identificateur_K_unit_rom={0,0,"_K",0,0};
  const identificateur & _IDNT_id_K_unit_rom=* (const identificateur *) &alias_identificateur_K_unit_rom;
  const alias_ref_identificateur ref_K_unit_rom={-1,0,0,"_K",0,0};
  const define_alias_gen(alias_K_unit_rom,_IDNT,0,&ref_K_unit_rom);
  const gen & K_unit_rom_IDNT_e = * (gen *) & alias_K_unit_rom;

  static const alias_identificateur alias_identificateur_L_unit_rom={0,0,"_L",0,0};
  const identificateur & _IDNT_id_L_unit_rom=* (const identificateur *) &alias_identificateur_L_unit_rom;
  const alias_ref_identificateur ref_L_unit_rom={-1,0,0,"_L",0,0};
  const define_alias_gen(alias_L_unit_rom,_IDNT,0,&ref_L_unit_rom);
  const gen & L_unit_rom_IDNT_e = * (gen *) & alias_L_unit_rom;

  static const alias_identificateur alias_identificateur_N_unit_rom={0,0,"_N",0,0};
  const identificateur & _IDNT_id_N_unit_rom=* (const identificateur *) &alias_identificateur_N_unit_rom;
  const alias_ref_identificateur ref_N_unit_rom={-1,0,0,"_N",0,0};
  const define_alias_gen(alias_N_unit_rom,_IDNT,0,&ref_N_unit_rom);
  const gen & N_unit_rom_IDNT_e = * (gen *) & alias_N_unit_rom;

  static const alias_identificateur alias_identificateur_NA__unit_rom={0,0,"_NA_",0,0};
  const identificateur & _IDNT_id_NA__unit_rom=* (const identificateur *) &alias_identificateur_NA__unit_rom;
  const alias_ref_identificateur ref_NA__unit_rom={-1,0,0,"_NA_",0,0};
  const define_alias_gen(alias_NA__unit_rom,_IDNT,0,&ref_NA__unit_rom);
  const gen & NA__unit_rom_IDNT_e = * (gen *) & alias_NA__unit_rom;

  static const alias_identificateur alias_identificateur_Ohm_unit_rom={0,0,"_Ohm",0,0};
  const identificateur & _IDNT_id_Ohm_unit_rom=* (const identificateur *) &alias_identificateur_Ohm_unit_rom;
  const alias_ref_identificateur ref_Ohm_unit_rom={-1,0,0,"_Ohm",0,0};
  const define_alias_gen(alias_Ohm_unit_rom,_IDNT,0,&ref_Ohm_unit_rom);
  const gen & Ohm_unit_rom_IDNT_e = * (gen *) & alias_Ohm_unit_rom;

  static const alias_identificateur alias_identificateur_P_unit_rom={0,0,"_P",0,0};
  const identificateur & _IDNT_id_P_unit_rom=* (const identificateur *) &alias_identificateur_P_unit_rom;
  const alias_ref_identificateur ref_P_unit_rom={-1,0,0,"_P",0,0};
  const define_alias_gen(alias_P_unit_rom,_IDNT,0,&ref_P_unit_rom);
  const gen & P_unit_rom_IDNT_e = * (gen *) & alias_P_unit_rom;

  static const alias_identificateur alias_identificateur_PSun__unit_rom={0,0,"_PSun_",0,0};
  const identificateur & _IDNT_id_PSun__unit_rom=* (const identificateur *) &alias_identificateur_PSun__unit_rom;
  const alias_ref_identificateur ref_PSun__unit_rom={-1,0,0,"_PSun_",0,0};
  const define_alias_gen(alias_PSun__unit_rom,_IDNT,0,&ref_PSun__unit_rom);
  const gen & PSun__unit_rom_IDNT_e = * (gen *) & alias_PSun__unit_rom;

  static const alias_identificateur alias_identificateur_Pa_unit_rom={0,0,"_Pa",0,0};
  const identificateur & _IDNT_id_Pa_unit_rom=* (const identificateur *) &alias_identificateur_Pa_unit_rom;
  const alias_ref_identificateur ref_Pa_unit_rom={-1,0,0,"_Pa",0,0};
  const define_alias_gen(alias_Pa_unit_rom,_IDNT,0,&ref_Pa_unit_rom);
  const gen & Pa_unit_rom_IDNT_e = * (gen *) & alias_Pa_unit_rom;

  static const alias_identificateur alias_identificateur_R_unit_rom={0,0,"_R",0,0};
  const identificateur & _IDNT_id_R_unit_rom=* (const identificateur *) &alias_identificateur_R_unit_rom;
  const alias_ref_identificateur ref_R_unit_rom={-1,0,0,"_R",0,0};
  const define_alias_gen(alias_R_unit_rom,_IDNT,0,&ref_R_unit_rom);
  const gen & R_unit_rom_IDNT_e = * (gen *) & alias_R_unit_rom;

  static const alias_identificateur alias_identificateur_Rankine_unit_rom={0,0,"_Rankine",0,0};
  const identificateur & _IDNT_id_Rankine_unit_rom=* (const identificateur *) &alias_identificateur_Rankine_unit_rom;
  const alias_ref_identificateur ref_Rankine_unit_rom={-1,0,0,"_Rankine",0,0};
  const define_alias_gen(alias_Rankine_unit_rom,_IDNT,0,&ref_Rankine_unit_rom);
  const gen & Rankine_unit_rom_IDNT_e = * (gen *) & alias_Rankine_unit_rom;

  static const alias_identificateur alias_identificateur_Rinfinity__unit_rom={0,0,"_Rinfinity_",0,0};
  const identificateur & _IDNT_id_Rinfinity__unit_rom=* (const identificateur *) &alias_identificateur_Rinfinity__unit_rom;
  const alias_ref_identificateur ref_Rinfinity__unit_rom={-1,0,0,"_Rinfinity_",0,0};
  const define_alias_gen(alias_Rinfinity__unit_rom,_IDNT,0,&ref_Rinfinity__unit_rom);
  const gen & Rinfinity__unit_rom_IDNT_e = * (gen *) & alias_Rinfinity__unit_rom;

  static const alias_identificateur alias_identificateur_S_unit_rom={0,0,"_S",0,0};
  const identificateur & _IDNT_id_S_unit_rom=* (const identificateur *) &alias_identificateur_S_unit_rom;
  const alias_ref_identificateur ref_S_unit_rom={-1,0,0,"_S",0,0};
  const define_alias_gen(alias_S_unit_rom,_IDNT,0,&ref_S_unit_rom);
  const gen & S_unit_rom_IDNT_e = * (gen *) & alias_S_unit_rom;

  static const alias_identificateur alias_identificateur_St_unit_rom={0,0,"_St",0,0};
  const identificateur & _IDNT_id_St_unit_rom=* (const identificateur *) &alias_identificateur_St_unit_rom;
  const alias_ref_identificateur ref_St_unit_rom={-1,0,0,"_St",0,0};
  const define_alias_gen(alias_St_unit_rom,_IDNT,0,&ref_St_unit_rom);
  const gen & St_unit_rom_IDNT_e = * (gen *) & alias_St_unit_rom;

  static const alias_identificateur alias_identificateur_StdP__unit_rom={0,0,"_StdP_",0,0};
  const identificateur & _IDNT_id_StdP__unit_rom=* (const identificateur *) &alias_identificateur_StdP__unit_rom;
  const alias_ref_identificateur ref_StdP__unit_rom={-1,0,0,"_StdP_",0,0};
  const define_alias_gen(alias_StdP__unit_rom,_IDNT,0,&ref_StdP__unit_rom);
  const gen & StdP__unit_rom_IDNT_e = * (gen *) & alias_StdP__unit_rom;

  static const alias_identificateur alias_identificateur_StdT__unit_rom={0,0,"_StdT_",0,0};
  const identificateur & _IDNT_id_StdT__unit_rom=* (const identificateur *) &alias_identificateur_StdT__unit_rom;
  const alias_ref_identificateur ref_StdT__unit_rom={-1,0,0,"_StdT_",0,0};
  const define_alias_gen(alias_StdT__unit_rom,_IDNT,0,&ref_StdT__unit_rom);
  const gen & StdT__unit_rom_IDNT_e = * (gen *) & alias_StdT__unit_rom;

  static const alias_identificateur alias_identificateur_Sv_unit_rom={0,0,"_Sv",0,0};
  const identificateur & _IDNT_id_Sv_unit_rom=* (const identificateur *) &alias_identificateur_Sv_unit_rom;
  const alias_ref_identificateur ref_Sv_unit_rom={-1,0,0,"_Sv",0,0};
  const define_alias_gen(alias_Sv_unit_rom,_IDNT,0,&ref_Sv_unit_rom);
  const gen & Sv_unit_rom_IDNT_e = * (gen *) & alias_Sv_unit_rom;

  static const alias_identificateur alias_identificateur_T_unit_rom={0,0,"_T",0,0};
  const identificateur & _IDNT_id_T_unit_rom=* (const identificateur *) &alias_identificateur_T_unit_rom;
  const alias_ref_identificateur ref_T_unit_rom={-1,0,0,"_T",0,0};
  const define_alias_gen(alias_T_unit_rom,_IDNT,0,&ref_T_unit_rom);
  const gen & T_unit_rom_IDNT_e = * (gen *) & alias_T_unit_rom;

  static const alias_identificateur alias_identificateur_Torr_unit_rom={0,0,"_Torr",0,0};
  const identificateur & _IDNT_id_Torr_unit_rom=* (const identificateur *) &alias_identificateur_Torr_unit_rom;
  const alias_ref_identificateur ref_Torr_unit_rom={-1,0,0,"_Torr",0,0};
  const define_alias_gen(alias_Torr_unit_rom,_IDNT,0,&ref_Torr_unit_rom);
  const gen & Torr_unit_rom_IDNT_e = * (gen *) & alias_Torr_unit_rom;

  static const alias_identificateur alias_identificateur_V_unit_rom={0,0,"_V",0,0};
  const identificateur & _IDNT_id_V_unit_rom=* (const identificateur *) &alias_identificateur_V_unit_rom;
  const alias_ref_identificateur ref_V_unit_rom={-1,0,0,"_V",0,0};
  const define_alias_gen(alias_V_unit_rom,_IDNT,0,&ref_V_unit_rom);
  const gen & V_unit_rom_IDNT_e = * (gen *) & alias_V_unit_rom;

  static const alias_identificateur alias_identificateur_Vm__unit_rom={0,0,"_Vm_",0,0};
  const identificateur & _IDNT_id_Vm__unit_rom=* (const identificateur *) &alias_identificateur_Vm__unit_rom;
  const alias_ref_identificateur ref_Vm__unit_rom={-1,0,0,"_Vm_",0,0};
  const define_alias_gen(alias_Vm__unit_rom,_IDNT,0,&ref_Vm__unit_rom);
  const gen & Vm__unit_rom_IDNT_e = * (gen *) & alias_Vm__unit_rom;

  static const alias_identificateur alias_identificateur_W_unit_rom={0,0,"_W",0,0};
  const identificateur & _IDNT_id_W_unit_rom=* (const identificateur *) &alias_identificateur_W_unit_rom;
  const alias_ref_identificateur ref_W_unit_rom={-1,0,0,"_W",0,0};
  const define_alias_gen(alias_W_unit_rom,_IDNT,0,&ref_W_unit_rom);
  const gen & W_unit_rom_IDNT_e = * (gen *) & alias_W_unit_rom;

  static const alias_identificateur alias_identificateur_Wb_unit_rom={0,0,"_Wb",0,0};
  const identificateur & _IDNT_id_Wb_unit_rom=* (const identificateur *) &alias_identificateur_Wb_unit_rom;
  const alias_ref_identificateur ref_Wb_unit_rom={-1,0,0,"_Wb",0,0};
  const define_alias_gen(alias_Wb_unit_rom,_IDNT,0,&ref_Wb_unit_rom);
  const gen & Wb_unit_rom_IDNT_e = * (gen *) & alias_Wb_unit_rom;

  static const alias_identificateur alias_identificateur_Wh_unit_rom={0,0,"_Wh",0,0};
  const identificateur & _IDNT_id_Wh_unit_rom=* (const identificateur *) &alias_identificateur_Wh_unit_rom;
  const alias_ref_identificateur ref_Wh_unit_rom={-1,0,0,"_Wh",0,0};
  const define_alias_gen(alias_Wh_unit_rom,_IDNT,0,&ref_Wh_unit_rom);
  const gen & Wh_unit_rom_IDNT_e = * (gen *) & alias_Wh_unit_rom;

  static const alias_identificateur alias_identificateur_a_unit_rom={0,0,"_a",0,0};
  const identificateur & _IDNT_id_a_unit_rom=* (const identificateur *) &alias_identificateur_a_unit_rom;
  const alias_ref_identificateur ref_a_unit_rom={-1,0,0,"_a",0,0};
  const define_alias_gen(alias_a_unit_rom,_IDNT,0,&ref_a_unit_rom);
  const gen & a_unit_rom_IDNT_e = * (gen *) & alias_a_unit_rom;

  static const alias_identificateur alias_identificateur_acre_unit_rom={0,0,"_acre",0,0};
  const identificateur & _IDNT_id_acre_unit_rom=* (const identificateur *) &alias_identificateur_acre_unit_rom;
  const alias_ref_identificateur ref_acre_unit_rom={-1,0,0,"_acre",0,0};
  const define_alias_gen(alias_acre_unit_rom,_IDNT,0,&ref_acre_unit_rom);
  const gen & acre_unit_rom_IDNT_e = * (gen *) & alias_acre_unit_rom;

  static const alias_identificateur alias_identificateur_alpha__unit_rom={0,0,"_alpha_",0,0};
  const identificateur & _IDNT_id_alpha__unit_rom=* (const identificateur *) &alias_identificateur_alpha__unit_rom;
  const alias_ref_identificateur ref_alpha__unit_rom={-1,0,0,"_alpha_",0,0};
  const define_alias_gen(alias_alpha__unit_rom,_IDNT,0,&ref_alpha__unit_rom);
  const gen & alpha__unit_rom_IDNT_e = * (gen *) & alias_alpha__unit_rom;

  static const alias_identificateur alias_identificateur_arcmin_unit_rom={0,0,"_arcmin",0,0};
  const identificateur & _IDNT_id_arcmin_unit_rom=* (const identificateur *) &alias_identificateur_arcmin_unit_rom;
  const alias_ref_identificateur ref_arcmin_unit_rom={-1,0,0,"_arcmin",0,0};
  const define_alias_gen(alias_arcmin_unit_rom,_IDNT,0,&ref_arcmin_unit_rom);
  const gen & arcmin_unit_rom_IDNT_e = * (gen *) & alias_arcmin_unit_rom;

  static const alias_identificateur alias_identificateur_arcs_unit_rom={0,0,"_arcs",0,0};
  const identificateur & _IDNT_id_arcs_unit_rom=* (const identificateur *) &alias_identificateur_arcs_unit_rom;
  const alias_ref_identificateur ref_arcs_unit_rom={-1,0,0,"_arcs",0,0};
  const define_alias_gen(alias_arcs_unit_rom,_IDNT,0,&ref_arcs_unit_rom);
  const gen & arcs_unit_rom_IDNT_e = * (gen *) & alias_arcs_unit_rom;

  static const alias_identificateur alias_identificateur_atm_unit_rom={0,0,"_atm",0,0};
  const identificateur & _IDNT_id_atm_unit_rom=* (const identificateur *) &alias_identificateur_atm_unit_rom;
  const alias_ref_identificateur ref_atm_unit_rom={-1,0,0,"_atm",0,0};
  const define_alias_gen(alias_atm_unit_rom,_IDNT,0,&ref_atm_unit_rom);
  const gen & atm_unit_rom_IDNT_e = * (gen *) & alias_atm_unit_rom;

  static const alias_identificateur alias_identificateur_au_unit_rom={0,0,"_au",0,0};
  const identificateur & _IDNT_id_au_unit_rom=* (const identificateur *) &alias_identificateur_au_unit_rom;
  const alias_ref_identificateur ref_au_unit_rom={-1,0,0,"_au",0,0};
  const define_alias_gen(alias_au_unit_rom,_IDNT,0,&ref_au_unit_rom);
  const gen & au_unit_rom_IDNT_e = * (gen *) & alias_au_unit_rom;

  static const alias_identificateur alias_identificateur_bar_unit_rom={0,0,"_bar",0,0};
  const identificateur & _IDNT_id_bar_unit_rom=* (const identificateur *) &alias_identificateur_bar_unit_rom;
  const alias_ref_identificateur ref_bar_unit_rom={-1,0,0,"_bar",0,0};
  const define_alias_gen(alias_bar_unit_rom,_IDNT,0,&ref_bar_unit_rom);
  const gen & bar_unit_rom_IDNT_e = * (gen *) & alias_bar_unit_rom;

  static const alias_identificateur alias_identificateur_b_unit_rom={0,0,"_b",0,0};
  const identificateur & _IDNT_id_b_unit_rom=* (const identificateur *) &alias_identificateur_b_unit_rom;
  const alias_ref_identificateur ref_b_unit_rom={-1,0,0,"_b",0,0};
  const define_alias_gen(alias_b_unit_rom,_IDNT,0,&ref_b_unit_rom);
  const gen & b_unit_rom_IDNT_e = * (gen *) & alias_b_unit_rom;

  static const alias_identificateur alias_identificateur_bbl_unit_rom={0,0,"_bbl",0,0};
  const identificateur & _IDNT_id_bbl_unit_rom=* (const identificateur *) &alias_identificateur_bbl_unit_rom;
  const alias_ref_identificateur ref_bbl_unit_rom={-1,0,0,"_bbl",0,0};
  const define_alias_gen(alias_bbl_unit_rom,_IDNT,0,&ref_bbl_unit_rom);
  const gen & bbl_unit_rom_IDNT_e = * (gen *) & alias_bbl_unit_rom;

  static const alias_identificateur alias_identificateur_bblep_unit_rom={0,0,"_bblep",0,0};
  const identificateur & _IDNT_id_bblep_unit_rom=* (const identificateur *) &alias_identificateur_bblep_unit_rom;
  const alias_ref_identificateur ref_bblep_unit_rom={-1,0,0,"_bblep",0,0};
  const define_alias_gen(alias_bblep_unit_rom,_IDNT,0,&ref_bblep_unit_rom);
  const gen & bblep_unit_rom_IDNT_e = * (gen *) & alias_bblep_unit_rom;

  static const alias_identificateur alias_identificateur_boe_unit_rom={0,0,"_boe",0,0};
  const identificateur & _IDNT_id_boe_unit_rom=* (const identificateur *) &alias_identificateur_boe_unit_rom;
  const alias_ref_identificateur ref_boe_unit_rom={-1,0,0,"_boe",0,0};
  const define_alias_gen(alias_boe_unit_rom,_IDNT,0,&ref_boe_unit_rom);
  const gen & boe_unit_rom_IDNT_e = * (gen *) & alias_boe_unit_rom;

  static const alias_identificateur alias_identificateur_bu_unit_rom={0,0,"_bu",0,0};
  const identificateur & _IDNT_id_bu_unit_rom=* (const identificateur *) &alias_identificateur_bu_unit_rom;
  const alias_ref_identificateur ref_bu_unit_rom={-1,0,0,"_bu",0,0};
  const define_alias_gen(alias_bu_unit_rom,_IDNT,0,&ref_bu_unit_rom);
  const gen & bu_unit_rom_IDNT_e = * (gen *) & alias_bu_unit_rom;

  static const alias_identificateur alias_identificateur_buUS_unit_rom={0,0,"_buUS",0,0};
  const identificateur & _IDNT_id_buUS_unit_rom=* (const identificateur *) &alias_identificateur_buUS_unit_rom;
  const alias_ref_identificateur ref_buUS_unit_rom={-1,0,0,"_buUS",0,0};
  const define_alias_gen(alias_buUS_unit_rom,_IDNT,0,&ref_buUS_unit_rom);
  const gen & buUS_unit_rom_IDNT_e = * (gen *) & alias_buUS_unit_rom;

  static const alias_identificateur alias_identificateur_c3__unit_rom={0,0,"_c3_",0,0};
  const identificateur & _IDNT_id_c3__unit_rom=* (const identificateur *) &alias_identificateur_c3__unit_rom;
  const alias_ref_identificateur ref_c3__unit_rom={-1,0,0,"_c3_",0,0};
  const define_alias_gen(alias_c3__unit_rom,_IDNT,0,&ref_c3__unit_rom);
  const gen & c3__unit_rom_IDNT_e = * (gen *) & alias_c3__unit_rom;

  static const alias_identificateur alias_identificateur_c__unit_rom={0,0,"_c_",0,0};
  const identificateur & _IDNT_id_c__unit_rom=* (const identificateur *) &alias_identificateur_c__unit_rom;
  const alias_ref_identificateur ref_c__unit_rom={-1,0,0,"_c_",0,0};
  const define_alias_gen(alias_c__unit_rom,_IDNT,0,&ref_c__unit_rom);
  const gen & c__unit_rom_IDNT_e = * (gen *) & alias_c__unit_rom;

  static const alias_identificateur alias_identificateur_cal_unit_rom={0,0,"_cal",0,0};
  const identificateur & _IDNT_id_cal_unit_rom=* (const identificateur *) &alias_identificateur_cal_unit_rom;
  const alias_ref_identificateur ref_cal_unit_rom={-1,0,0,"_cal",0,0};
  const define_alias_gen(alias_cal_unit_rom,_IDNT,0,&ref_cal_unit_rom);
  const gen & cal_unit_rom_IDNT_e = * (gen *) & alias_cal_unit_rom;

  static const alias_identificateur alias_identificateur_cd_unit_rom={0,0,"_cd",0,0};
  const identificateur & _IDNT_id_cd_unit_rom=* (const identificateur *) &alias_identificateur_cd_unit_rom;
  const alias_ref_identificateur ref_cd_unit_rom={-1,0,0,"_cd",0,0};
  const define_alias_gen(alias_cd_unit_rom,_IDNT,0,&ref_cd_unit_rom);
  const gen & cd_unit_rom_IDNT_e = * (gen *) & alias_cd_unit_rom;

  static const alias_identificateur alias_identificateur_cf_unit_rom={0,0,"_cf",0,0};
  const identificateur & _IDNT_id_cf_unit_rom=* (const identificateur *) &alias_identificateur_cf_unit_rom;
  const alias_ref_identificateur ref_cf_unit_rom={-1,0,0,"_cf",0,0};
  const define_alias_gen(alias_cf_unit_rom,_IDNT,0,&ref_cf_unit_rom);
  const gen & cf_unit_rom_IDNT_e = * (gen *) & alias_cf_unit_rom;

  static const alias_identificateur alias_identificateur_chain_unit_rom={0,0,"_chain",0,0};
  const identificateur & _IDNT_id_chain_unit_rom=* (const identificateur *) &alias_identificateur_chain_unit_rom;
  const alias_ref_identificateur ref_chain_unit_rom={-1,0,0,"_chain",0,0};
  const define_alias_gen(alias_chain_unit_rom,_IDNT,0,&ref_chain_unit_rom);
  const gen & chain_unit_rom_IDNT_e = * (gen *) & alias_chain_unit_rom;

  static const alias_identificateur alias_identificateur_ct_unit_rom={0,0,"_ct",0,0};
  const identificateur & _IDNT_id_ct_unit_rom=* (const identificateur *) &alias_identificateur_ct_unit_rom;
  const alias_ref_identificateur ref_ct_unit_rom={-1,0,0,"_ct",0,0};
  const define_alias_gen(alias_ct_unit_rom,_IDNT,0,&ref_ct_unit_rom);
  const gen & ct_unit_rom_IDNT_e = * (gen *) & alias_ct_unit_rom;

  static const alias_identificateur alias_identificateur_cu_unit_rom={0,0,"_cu",0,0};
  const identificateur & _IDNT_id_cu_unit_rom=* (const identificateur *) &alias_identificateur_cu_unit_rom;
  const alias_ref_identificateur ref_cu_unit_rom={-1,0,0,"_cu",0,0};
  const define_alias_gen(alias_cu_unit_rom,_IDNT,0,&ref_cu_unit_rom);
  const gen & cu_unit_rom_IDNT_e = * (gen *) & alias_cu_unit_rom;

  static const alias_identificateur alias_identificateur_d_unit_rom={0,0,"_d",0,0};
  const identificateur & _IDNT_id_d_unit_rom=* (const identificateur *) &alias_identificateur_d_unit_rom;
  const alias_ref_identificateur ref_d_unit_rom={-1,0,0,"_d",0,0};
  const define_alias_gen(alias_d_unit_rom,_IDNT,0,&ref_d_unit_rom);
  const gen & d_unit_rom_IDNT_e = * (gen *) & alias_d_unit_rom;

  static const alias_identificateur alias_identificateur_dB_unit_rom={0,0,"_dB",0,0};
  const identificateur & _IDNT_id_dB_unit_rom=* (const identificateur *) &alias_identificateur_dB_unit_rom;
  const alias_ref_identificateur ref_dB_unit_rom={-1,0,0,"_dB",0,0};
  const define_alias_gen(alias_dB_unit_rom,_IDNT,0,&ref_dB_unit_rom);
  const gen & dB_unit_rom_IDNT_e = * (gen *) & alias_dB_unit_rom;

  static const alias_identificateur alias_identificateur_deg_unit_rom={0,0,"_deg",0,0};
  const identificateur & _IDNT_id_deg_unit_rom=* (const identificateur *) &alias_identificateur_deg_unit_rom;
  const alias_ref_identificateur ref_deg_unit_rom={-1,0,0,"_deg",0,0};
  const define_alias_gen(alias_deg_unit_rom,_IDNT,0,&ref_deg_unit_rom);
  const gen & deg_unit_rom_IDNT_e = * (gen *) & alias_deg_unit_rom;

  static const alias_identificateur alias_identificateur_dyn_unit_rom={0,0,"_dyn",0,0};
  const identificateur & _IDNT_id_dyn_unit_rom=* (const identificateur *) &alias_identificateur_dyn_unit_rom;
  const alias_ref_identificateur ref_dyn_unit_rom={-1,0,0,"_dyn",0,0};
  const define_alias_gen(alias_dyn_unit_rom,_IDNT,0,&ref_dyn_unit_rom);
  const gen & dyn_unit_rom_IDNT_e = * (gen *) & alias_dyn_unit_rom;

  static const alias_identificateur alias_identificateur_eV_unit_rom={0,0,"_eV",0,0};
  const identificateur & _IDNT_id_eV_unit_rom=* (const identificateur *) &alias_identificateur_eV_unit_rom;
  const alias_ref_identificateur ref_eV_unit_rom={-1,0,0,"_eV",0,0};
  const define_alias_gen(alias_eV_unit_rom,_IDNT,0,&ref_eV_unit_rom);
  const gen & eV_unit_rom_IDNT_e = * (gen *) & alias_eV_unit_rom;

  static const alias_identificateur alias_identificateur_epsilon0q__unit_rom={0,0,"_epsilon0q_",0,0};
  const identificateur & _IDNT_id_epsilon0q__unit_rom=* (const identificateur *) &alias_identificateur_epsilon0q__unit_rom;
  const alias_ref_identificateur ref_epsilon0q__unit_rom={-1,0,0,"_epsilon0q_",0,0};
  const define_alias_gen(alias_epsilon0q__unit_rom,_IDNT,0,&ref_epsilon0q__unit_rom);
  const gen & epsilon0q__unit_rom_IDNT_e = * (gen *) & alias_epsilon0q__unit_rom;

  static const alias_identificateur alias_identificateur_epsilonox__unit_rom={0,0,"_epsilonox",0,0};
  const identificateur & _IDNT_id_epsilonox__unit_rom=* (const identificateur *) &alias_identificateur_epsilonox__unit_rom;
  const alias_ref_identificateur ref_epsilonox__unit_rom={-1,0,0,"_epsilonox",0,0};
  const define_alias_gen(alias_epsilonox__unit_rom,_IDNT,0,&ref_epsilonox__unit_rom);
  const gen & epsilonox__unit_rom_IDNT_e = * (gen *) & alias_epsilonox__unit_rom;

  static const alias_identificateur alias_identificateur_epsilonsi__unit_rom={0,0,"_epsilonsi_",0,0};
  const identificateur & _IDNT_id_epsilonsi__unit_rom=* (const identificateur *) &alias_identificateur_epsilonsi__unit_rom;
  const alias_ref_identificateur ref_epsilonsi__unit_rom={-1,0,0,"_epsilonsi_",0,0};
  const define_alias_gen(alias_epsilonsi__unit_rom,_IDNT,0,&ref_epsilonsi__unit_rom);
  const gen & epsilonsi__unit_rom_IDNT_e = * (gen *) & alias_epsilonsi__unit_rom;

  static const alias_identificateur alias_identificateur_erg_unit_rom={0,0,"_erg",0,0};
  const identificateur & _IDNT_id_erg_unit_rom=* (const identificateur *) &alias_identificateur_erg_unit_rom;
  const alias_ref_identificateur ref_erg_unit_rom={-1,0,0,"_erg",0,0};
  const define_alias_gen(alias_erg_unit_rom,_IDNT,0,&ref_erg_unit_rom);
  const gen & erg_unit_rom_IDNT_e = * (gen *) & alias_erg_unit_rom;

  static const alias_identificateur alias_identificateur_f0__unit_rom={0,0,"_f0_",0,0};
  const identificateur & _IDNT_id_f0__unit_rom=* (const identificateur *) &alias_identificateur_f0__unit_rom;
  const alias_ref_identificateur ref_f0__unit_rom={-1,0,0,"_f0_",0,0};
  const define_alias_gen(alias_f0__unit_rom,_IDNT,0,&ref_f0__unit_rom);
  const gen & f0__unit_rom_IDNT_e = * (gen *) & alias_f0__unit_rom;

  static const alias_identificateur alias_identificateur_fath_unit_rom={0,0,"_fath",0,0};
  const identificateur & _IDNT_id_fath_unit_rom=* (const identificateur *) &alias_identificateur_fath_unit_rom;
  const alias_ref_identificateur ref_fath_unit_rom={-1,0,0,"_fath",0,0};
  const define_alias_gen(alias_fath_unit_rom,_IDNT,0,&ref_fath_unit_rom);
  const gen & fath_unit_rom_IDNT_e = * (gen *) & alias_fath_unit_rom;

  static const alias_identificateur alias_identificateur_fbm_unit_rom={0,0,"_fbm",0,0};
  const identificateur & _IDNT_id_fbm_unit_rom=* (const identificateur *) &alias_identificateur_fbm_unit_rom;
  const alias_ref_identificateur ref_fbm_unit_rom={-1,0,0,"_fbm",0,0};
  const define_alias_gen(alias_fbm_unit_rom,_IDNT,0,&ref_fbm_unit_rom);
  const gen & fbm_unit_rom_IDNT_e = * (gen *) & alias_fbm_unit_rom;

  static const alias_identificateur alias_identificateur_fc_unit_rom={0,0,"_fc",0,0};
  const identificateur & _IDNT_id_fc_unit_rom=* (const identificateur *) &alias_identificateur_fc_unit_rom;
  const alias_ref_identificateur ref_fc_unit_rom={-1,0,0,"_fc",0,0};
  const define_alias_gen(alias_fc_unit_rom,_IDNT,0,&ref_fc_unit_rom);
  const gen & fc_unit_rom_IDNT_e = * (gen *) & alias_fc_unit_rom;

  static const alias_identificateur alias_identificateur_fermi_unit_rom={0,0,"_fermi",0,0};
  const identificateur & _IDNT_id_fermi_unit_rom=* (const identificateur *) &alias_identificateur_fermi_unit_rom;
  const alias_ref_identificateur ref_fermi_unit_rom={-1,0,0,"_fermi",0,0};
  const define_alias_gen(alias_fermi_unit_rom,_IDNT,0,&ref_fermi_unit_rom);
  const gen & fermi_unit_rom_IDNT_e = * (gen *) & alias_fermi_unit_rom;

  static const alias_identificateur alias_identificateur_flam_unit_rom={0,0,"_flam",0,0};
  const identificateur & _IDNT_id_flam_unit_rom=* (const identificateur *) &alias_identificateur_flam_unit_rom;
  const alias_ref_identificateur ref_flam_unit_rom={-1,0,0,"_flam",0,0};
  const define_alias_gen(alias_flam_unit_rom,_IDNT,0,&ref_flam_unit_rom);
  const gen & flam_unit_rom_IDNT_e = * (gen *) & alias_flam_unit_rom;

  static const alias_identificateur alias_identificateur_fm_unit_rom={0,0,"_fm",0,0};
  const identificateur & _IDNT_id_fm_unit_rom=* (const identificateur *) &alias_identificateur_fm_unit_rom;
  const alias_ref_identificateur ref_fm_unit_rom={-1,0,0,"_fm",0,0};
  const define_alias_gen(alias_fm_unit_rom,_IDNT,0,&ref_fm_unit_rom);
  const gen & fm_unit_rom_IDNT_e = * (gen *) & alias_fm_unit_rom;

  static const alias_identificateur alias_identificateur_ft_unit_rom={0,0,"_ft",0,0};
  const identificateur & _IDNT_id_ft_unit_rom=* (const identificateur *) &alias_identificateur_ft_unit_rom;
  const alias_ref_identificateur ref_ft_unit_rom={-1,0,0,"_ft",0,0};
  const define_alias_gen(alias_ft_unit_rom,_IDNT,0,&ref_ft_unit_rom);
  const gen & ft_unit_rom_IDNT_e = * (gen *) & alias_ft_unit_rom;

  static const alias_identificateur alias_identificateur_ftUS_unit_rom={0,0,"_ftUS",0,0};
  const identificateur & _IDNT_id_ftUS_unit_rom=* (const identificateur *) &alias_identificateur_ftUS_unit_rom;
  const alias_ref_identificateur ref_ftUS_unit_rom={-1,0,0,"_ftUS",0,0};
  const define_alias_gen(alias_ftUS_unit_rom,_IDNT,0,&ref_ftUS_unit_rom);
  const gen & ftUS_unit_rom_IDNT_e = * (gen *) & alias_ftUS_unit_rom;

  static const alias_identificateur alias_identificateur_g_unit_rom={0,0,"_g",0,0};
  const identificateur & _IDNT_id_g_unit_rom=* (const identificateur *) &alias_identificateur_g_unit_rom;
  const alias_ref_identificateur ref_g_unit_rom={-1,0,0,"_g",0,0};
  const define_alias_gen(alias_g_unit_rom,_IDNT,0,&ref_g_unit_rom);
  const gen & g_unit_rom_IDNT_e = * (gen *) & alias_g_unit_rom;

  static const alias_identificateur alias_identificateur_g__unit_rom={0,0,"_g_",0,0};
  const identificateur & _IDNT_id_g__unit_rom=* (const identificateur *) &alias_identificateur_g__unit_rom;
  const alias_ref_identificateur ref_g__unit_rom={-1,0,0,"_g_",0,0};
  const define_alias_gen(alias_g__unit_rom,_IDNT,0,&ref_g__unit_rom);
  const gen & g__unit_rom_IDNT_e = * (gen *) & alias_g__unit_rom;

  static const alias_identificateur alias_identificateur_galC_unit_rom={0,0,"_galC",0,0};
  const identificateur & _IDNT_id_galC_unit_rom=* (const identificateur *) &alias_identificateur_galC_unit_rom;
  const alias_ref_identificateur ref_galC_unit_rom={-1,0,0,"_galC",0,0};
  const define_alias_gen(alias_galC_unit_rom,_IDNT,0,&ref_galC_unit_rom);
  const gen & galC_unit_rom_IDNT_e = * (gen *) & alias_galC_unit_rom;

  static const alias_identificateur alias_identificateur_galUK_unit_rom={0,0,"_galUK",0,0};
  const identificateur & _IDNT_id_galUK_unit_rom=* (const identificateur *) &alias_identificateur_galUK_unit_rom;
  const alias_ref_identificateur ref_galUK_unit_rom={-1,0,0,"_galUK",0,0};
  const define_alias_gen(alias_galUK_unit_rom,_IDNT,0,&ref_galUK_unit_rom);
  const gen & galUK_unit_rom_IDNT_e = * (gen *) & alias_galUK_unit_rom;

  static const alias_identificateur alias_identificateur_galUS_unit_rom={0,0,"_galUS",0,0};
  const identificateur & _IDNT_id_galUS_unit_rom=* (const identificateur *) &alias_identificateur_galUS_unit_rom;
  const alias_ref_identificateur ref_galUS_unit_rom={-1,0,0,"_galUS",0,0};
  const define_alias_gen(alias_galUS_unit_rom,_IDNT,0,&ref_galUS_unit_rom);
  const gen & galUS_unit_rom_IDNT_e = * (gen *) & alias_galUS_unit_rom;

  static const alias_identificateur alias_identificateur_gf_unit_rom={0,0,"_gf",0,0};
  const identificateur & _IDNT_id_gf_unit_rom=* (const identificateur *) &alias_identificateur_gf_unit_rom;
  const alias_ref_identificateur ref_gf_unit_rom={-1,0,0,"_gf",0,0};
  const define_alias_gen(alias_gf_unit_rom,_IDNT,0,&ref_gf_unit_rom);
  const gen & gf_unit_rom_IDNT_e = * (gen *) & alias_gf_unit_rom;

  static const alias_identificateur alias_identificateur_gmol_unit_rom={0,0,"_gmol",0,0};
  const identificateur & _IDNT_id_gmol_unit_rom=* (const identificateur *) &alias_identificateur_gmol_unit_rom;
  const alias_ref_identificateur ref_gmol_unit_rom={-1,0,0,"_gmol",0,0};
  const define_alias_gen(alias_gmol_unit_rom,_IDNT,0,&ref_gmol_unit_rom);
  const gen & gmol_unit_rom_IDNT_e = * (gen *) & alias_gmol_unit_rom;

  static const alias_identificateur alias_identificateur_gon_unit_rom={0,0,"_gon",0,0};
  const identificateur & _IDNT_id_gon_unit_rom=* (const identificateur *) &alias_identificateur_gon_unit_rom;
  const alias_ref_identificateur ref_gon_unit_rom={-1,0,0,"_gon",0,0};
  const define_alias_gen(alias_gon_unit_rom,_IDNT,0,&ref_gon_unit_rom);
  const gen & gon_unit_rom_IDNT_e = * (gen *) & alias_gon_unit_rom;

  static const alias_identificateur alias_identificateur_grad_unit_rom={0,0,"_grad",0,0};
  const identificateur & _IDNT_id_grad_unit_rom=* (const identificateur *) &alias_identificateur_grad_unit_rom;
  const alias_ref_identificateur ref_grad_unit_rom={-1,0,0,"_grad",0,0};
  const define_alias_gen(alias_grad_unit_rom,_IDNT,0,&ref_grad_unit_rom);
  const gen & grad_unit_rom_IDNT_e = * (gen *) & alias_grad_unit_rom;

  static const alias_identificateur alias_identificateur_grain_unit_rom={0,0,"_grain",0,0};
  const identificateur & _IDNT_id_grain_unit_rom=* (const identificateur *) &alias_identificateur_grain_unit_rom;
  const alias_ref_identificateur ref_grain_unit_rom={-1,0,0,"_grain",0,0};
  const define_alias_gen(alias_grain_unit_rom,_IDNT,0,&ref_grain_unit_rom);
  const gen & grain_unit_rom_IDNT_e = * (gen *) & alias_grain_unit_rom;

  static const alias_identificateur alias_identificateur_h_unit_rom={0,0,"_h",0,0};
  const identificateur & _IDNT_id_h_unit_rom=* (const identificateur *) &alias_identificateur_h_unit_rom;
  const alias_ref_identificateur ref_h_unit_rom={-1,0,0,"_h",0,0};
  const define_alias_gen(alias_h_unit_rom,_IDNT,0,&ref_h_unit_rom);
  const gen & h_unit_rom_IDNT_e = * (gen *) & alias_h_unit_rom;

  static const alias_identificateur alias_identificateur_h__unit_rom={0,0,"_h_",0,0};
  const identificateur & _IDNT_id_h__unit_rom=* (const identificateur *) &alias_identificateur_h__unit_rom;
  const alias_ref_identificateur ref_h__unit_rom={-1,0,0,"_h_",0,0};
  const define_alias_gen(alias_h__unit_rom,_IDNT,0,&ref_h__unit_rom);
  const gen & h__unit_rom_IDNT_e = * (gen *) & alias_h__unit_rom;

  static const alias_identificateur alias_identificateur_ha_unit_rom={0,0,"_ha",0,0};
  const identificateur & _IDNT_id_ha_unit_rom=* (const identificateur *) &alias_identificateur_ha_unit_rom;
  const alias_ref_identificateur ref_ha_unit_rom={-1,0,0,"_ha",0,0};
  const define_alias_gen(alias_ha_unit_rom,_IDNT,0,&ref_ha_unit_rom);
  const gen & ha_unit_rom_IDNT_e = * (gen *) & alias_ha_unit_rom;

  static const alias_identificateur alias_identificateur_hbar__unit_rom={0,0,"_hbar_",0,0};
  const identificateur & _IDNT_id_hbar__unit_rom=* (const identificateur *) &alias_identificateur_hbar__unit_rom;
  const alias_ref_identificateur ref_hbar__unit_rom={-1,0,0,"_hbar_",0,0};
  const define_alias_gen(alias_hbar__unit_rom,_IDNT,0,&ref_hbar__unit_rom);
  const gen & hbar__unit_rom_IDNT_e = * (gen *) & alias_hbar__unit_rom;

  static const alias_identificateur alias_identificateur_hp_unit_rom={0,0,"_hp",0,0};
  const identificateur & _IDNT_id_hp_unit_rom=* (const identificateur *) &alias_identificateur_hp_unit_rom;
  const alias_ref_identificateur ref_hp_unit_rom={-1,0,0,"_hp",0,0};
  const define_alias_gen(alias_hp_unit_rom,_IDNT,0,&ref_hp_unit_rom);
  const gen & hp_unit_rom_IDNT_e = * (gen *) & alias_hp_unit_rom;

  static const alias_identificateur alias_identificateur_inH2O_unit_rom={0,0,"_inH2O",0,0};
  const identificateur & _IDNT_id_inH2O_unit_rom=* (const identificateur *) &alias_identificateur_inH2O_unit_rom;
  const alias_ref_identificateur ref_inH2O_unit_rom={-1,0,0,"_inH2O",0,0};
  const define_alias_gen(alias_inH2O_unit_rom,_IDNT,0,&ref_inH2O_unit_rom);
  const gen & inH2O_unit_rom_IDNT_e = * (gen *) & alias_inH2O_unit_rom;

  static const alias_identificateur alias_identificateur_inHg_unit_rom={0,0,"_inHg",0,0};
  const identificateur & _IDNT_id_inHg_unit_rom=* (const identificateur *) &alias_identificateur_inHg_unit_rom;
  const alias_ref_identificateur ref_inHg_unit_rom={-1,0,0,"_inHg",0,0};
  const define_alias_gen(alias_inHg_unit_rom,_IDNT,0,&ref_inHg_unit_rom);
  const gen & inHg_unit_rom_IDNT_e = * (gen *) & alias_inHg_unit_rom;

  static const alias_identificateur alias_identificateur_inch_unit_rom={0,0,"_inch",0,0};
  const identificateur & _IDNT_id_inch_unit_rom=* (const identificateur *) &alias_identificateur_inch_unit_rom;
  const alias_ref_identificateur ref_inch_unit_rom={-1,0,0,"_inch",0,0};
  const define_alias_gen(alias_inch_unit_rom,_IDNT,0,&ref_inch_unit_rom);
  const gen & inch_unit_rom_IDNT_e = * (gen *) & alias_inch_unit_rom;

  static const alias_identificateur alias_identificateur_j_unit_rom={0,0,"_j",0,0};
  const identificateur & _IDNT_id_j_unit_rom=* (const identificateur *) &alias_identificateur_j_unit_rom;
  const alias_ref_identificateur ref_j_unit_rom={-1,0,0,"_j",0,0};
  const define_alias_gen(alias_j_unit_rom,_IDNT,0,&ref_j_unit_rom);
  const gen & j_unit_rom_IDNT_e = * (gen *) & alias_j_unit_rom;

  static const alias_identificateur alias_identificateur_k__unit_rom={0,0,"_k_",0,0};
  const identificateur & _IDNT_id_k__unit_rom=* (const identificateur *) &alias_identificateur_k__unit_rom;
  const alias_ref_identificateur ref_k__unit_rom={-1,0,0,"_k_",0,0};
  const define_alias_gen(alias_k__unit_rom,_IDNT,0,&ref_k__unit_rom);
  const gen & k__unit_rom_IDNT_e = * (gen *) & alias_k__unit_rom;

  static const alias_identificateur alias_identificateur_kip_unit_rom={0,0,"_kip",0,0};
  const identificateur & _IDNT_id_kip_unit_rom=* (const identificateur *) &alias_identificateur_kip_unit_rom;
  const alias_ref_identificateur ref_kip_unit_rom={-1,0,0,"_kip",0,0};
  const define_alias_gen(alias_kip_unit_rom,_IDNT,0,&ref_kip_unit_rom);
  const gen & kip_unit_rom_IDNT_e = * (gen *) & alias_kip_unit_rom;

  static const alias_identificateur alias_identificateur_knot_unit_rom={0,0,"_knot",0,0};
  const identificateur & _IDNT_id_knot_unit_rom=* (const identificateur *) &alias_identificateur_knot_unit_rom;
  const alias_ref_identificateur ref_knot_unit_rom={-1,0,0,"_knot",0,0};
  const define_alias_gen(alias_knot_unit_rom,_IDNT,0,&ref_knot_unit_rom);
  const gen & knot_unit_rom_IDNT_e = * (gen *) & alias_knot_unit_rom;

  static const alias_identificateur alias_identificateur_kph_unit_rom={0,0,"_kph",0,0};
  const identificateur & _IDNT_id_kph_unit_rom=* (const identificateur *) &alias_identificateur_kph_unit_rom;
  const alias_ref_identificateur ref_kph_unit_rom={-1,0,0,"_kph",0,0};
  const define_alias_gen(alias_kph_unit_rom,_IDNT,0,&ref_kph_unit_rom);
  const gen & kph_unit_rom_IDNT_e = * (gen *) & alias_kph_unit_rom;

  static const alias_identificateur alias_identificateur_kq__unit_rom={0,0,"_kq",0,0};
  const identificateur & _IDNT_id_kq__unit_rom=* (const identificateur *) &alias_identificateur_kq__unit_rom;
  const alias_ref_identificateur ref_kq__unit_rom={-1,0,0,"_kq_",0,0};
  const define_alias_gen(alias_kq__unit_rom,_IDNT,0,&ref_kq__unit_rom);
  const gen & kq__unit_rom_IDNT_e = * (gen *) & alias_kq__unit_rom;

  static const alias_identificateur alias_identificateur_l_unit_rom={0,0,"_l",0,0};
  const identificateur & _IDNT_id_l_unit_rom=* (const identificateur *) &alias_identificateur_l_unit_rom;
  const alias_ref_identificateur ref_l_unit_rom={-1,0,0,"_l",0,0};
  const define_alias_gen(alias_l_unit_rom,_IDNT,0,&ref_l_unit_rom);
  const gen & l_unit_rom_IDNT_e = * (gen *) & alias_l_unit_rom;

  static const alias_identificateur alias_identificateur_lam_unit_rom={0,0,"_lam",0,0};
  const identificateur & _IDNT_id_lam_unit_rom=* (const identificateur *) &alias_identificateur_lam_unit_rom;
  const alias_ref_identificateur ref_lam_unit_rom={-1,0,0,"_lam",0,0};
  const define_alias_gen(alias_lam_unit_rom,_IDNT,0,&ref_lam_unit_rom);
  const gen & lam_unit_rom_IDNT_e = * (gen *) & alias_lam_unit_rom;

  static const alias_identificateur alias_identificateur_lambda0_unit_rom={0,0,"_lambda0",0,0};
  const identificateur & _IDNT_id_lambda0_unit_rom=* (const identificateur *) &alias_identificateur_lambda0_unit_rom;
  const alias_ref_identificateur ref_lambda0_unit_rom={-1,0,0,"_lambda0",0,0};
  const define_alias_gen(alias_lambda0_unit_rom,_IDNT,0,&ref_lambda0_unit_rom);
  const gen & lambda0_unit_rom_IDNT_e = * (gen *) & alias_lambda0_unit_rom;

  static const alias_identificateur alias_identificateur_lambdac__unit_rom={0,0,"_lambdac_",0,0};
  const identificateur & _IDNT_id_lambdac__unit_rom=* (const identificateur *) &alias_identificateur_lambdac__unit_rom;
  const alias_ref_identificateur ref_lambdac__unit_rom={-1,0,0,"_lambdac_",0,0};
  const define_alias_gen(alias_lambdac__unit_rom,_IDNT,0,&ref_lambdac__unit_rom);
  const gen & lambdac__unit_rom_IDNT_e = * (gen *) & alias_lambdac__unit_rom;

  static const alias_identificateur alias_identificateur_lb_unit_rom={0,0,"_lb",0,0};
  const identificateur & _IDNT_id_lb_unit_rom=* (const identificateur *) &alias_identificateur_lb_unit_rom;
  const alias_ref_identificateur ref_lb_unit_rom={-1,0,0,"_lb",0,0};
  const define_alias_gen(alias_lb_unit_rom,_IDNT,0,&ref_lb_unit_rom);
  const gen & lb_unit_rom_IDNT_e = * (gen *) & alias_lb_unit_rom;

  static const alias_identificateur alias_identificateur_lbf_unit_rom={0,0,"_lbf",0,0};
  const identificateur & _IDNT_id_lbf_unit_rom=* (const identificateur *) &alias_identificateur_lbf_unit_rom;
  const alias_ref_identificateur ref_lbf_unit_rom={-1,0,0,"_lbf",0,0};
  const define_alias_gen(alias_lbf_unit_rom,_IDNT,0,&ref_lbf_unit_rom);
  const gen & lbf_unit_rom_IDNT_e = * (gen *) & alias_lbf_unit_rom;

  static const alias_identificateur alias_identificateur_lbmol_unit_rom={0,0,"_lbmol",0,0};
  const identificateur & _IDNT_id_lbmol_unit_rom=* (const identificateur *) &alias_identificateur_lbmol_unit_rom;
  const alias_ref_identificateur ref_lbmol_unit_rom={-1,0,0,"_lbmol",0,0};
  const define_alias_gen(alias_lbmol_unit_rom,_IDNT,0,&ref_lbmol_unit_rom);
  const gen & lbmol_unit_rom_IDNT_e = * (gen *) & alias_lbmol_unit_rom;

  static const alias_identificateur alias_identificateur_lbt_unit_rom={0,0,"_lbt",0,0};
  const identificateur & _IDNT_id_lbt_unit_rom=* (const identificateur *) &alias_identificateur_lbt_unit_rom;
  const alias_ref_identificateur ref_lbt_unit_rom={-1,0,0,"_lbt",0,0};
  const define_alias_gen(alias_lbt_unit_rom,_IDNT,0,&ref_lbt_unit_rom);
  const gen & lbt_unit_rom_IDNT_e = * (gen *) & alias_lbt_unit_rom;

  static const alias_identificateur alias_identificateur_lep_unit_rom={0,0,"_lep",0,0};
  const identificateur & _IDNT_id_lep_unit_rom=* (const identificateur *) &alias_identificateur_lep_unit_rom;
  const alias_ref_identificateur ref_lep_unit_rom={-1,0,0,"_lep",0,0};
  const define_alias_gen(alias_lep_unit_rom,_IDNT,0,&ref_lep_unit_rom);
  const gen & lep_unit_rom_IDNT_e = * (gen *) & alias_lep_unit_rom;

  static const alias_identificateur alias_identificateur_liqpt_unit_rom={0,0,"_liqpt",0,0};
  const identificateur & _IDNT_id_liqpt_unit_rom=* (const identificateur *) &alias_identificateur_liqpt_unit_rom;
  const alias_ref_identificateur ref_liqpt_unit_rom={-1,0,0,"_liqpt",0,0};
  const define_alias_gen(alias_liqpt_unit_rom,_IDNT,0,&ref_liqpt_unit_rom);
  const gen & liqpt_unit_rom_IDNT_e = * (gen *) & alias_liqpt_unit_rom;

  static const alias_identificateur alias_identificateur_lyr_unit_rom={0,0,"_lyr",0,0};
  const identificateur & _IDNT_id_lyr_unit_rom=* (const identificateur *) &alias_identificateur_lyr_unit_rom;
  const alias_ref_identificateur ref_lyr_unit_rom={-1,0,0,"_lyr",0,0};
  const define_alias_gen(alias_lyr_unit_rom,_IDNT,0,&ref_lyr_unit_rom);
  const gen & lyr_unit_rom_IDNT_e = * (gen *) & alias_lyr_unit_rom;

  static const alias_identificateur alias_identificateur_m_unit_rom={0,0,"_m",0,0};
  const identificateur & _IDNT_id_m_unit_rom=* (const identificateur *) &alias_identificateur_m_unit_rom;
  const alias_ref_identificateur ref_m_unit_rom={-1,0,0,"_m",0,0};
  const define_alias_gen(alias_m_unit_rom,_IDNT,0,&ref_m_unit_rom);
  const gen & m_unit_rom_IDNT_e = * (gen *) & alias_m_unit_rom;

  static const alias_identificateur alias_identificateur_mEarth__unit_rom={0,0,"_mEarth_",0,0};
  const identificateur & _IDNT_id_mEarth__unit_rom=* (const identificateur *) &alias_identificateur_mEarth__unit_rom;
  const alias_ref_identificateur ref_mEarth__unit_rom={-1,0,0,"_mEarth_",0,0};
  const define_alias_gen(alias_mEarth__unit_rom,_IDNT,0,&ref_mEarth__unit_rom);
  const gen & mEarth__unit_rom_IDNT_e = * (gen *) & alias_mEarth__unit_rom;

  static const alias_identificateur alias_identificateur_mSun__unit_rom={0,0,"_mSun_",0,0};
  const identificateur & _IDNT_id_mSun__unit_rom=* (const identificateur *) &alias_identificateur_mSun__unit_rom;
  const alias_ref_identificateur ref_mSun__unit_rom={-1,0,0,"_mSun_",0,0};
  const define_alias_gen(alias_mSun__unit_rom,_IDNT,0,&ref_mSun__unit_rom);
  const gen & mSun__unit_rom_IDNT_e = * (gen *) & alias_mSun__unit_rom;

  static const alias_identificateur alias_identificateur_me__unit_rom={0,0,"_me_",0,0};
  const identificateur & _IDNT_id_me__unit_rom=* (const identificateur *) &alias_identificateur_me__unit_rom;
  const alias_ref_identificateur ref_me__unit_rom={-1,0,0,"_me_",0,0};
  const define_alias_gen(alias_me__unit_rom,_IDNT,0,&ref_me__unit_rom);
  const gen & me__unit_rom_IDNT_e = * (gen *) & alias_me__unit_rom;

  static const alias_identificateur alias_identificateur_mho_unit_rom={0,0,"_mho",0,0};
  const identificateur & _IDNT_id_mho_unit_rom=* (const identificateur *) &alias_identificateur_mho_unit_rom;
  const alias_ref_identificateur ref_mho_unit_rom={-1,0,0,"_mho",0,0};
  const define_alias_gen(alias_mho_unit_rom,_IDNT,0,&ref_mho_unit_rom);
  const gen & mho_unit_rom_IDNT_e = * (gen *) & alias_mho_unit_rom;

  static const alias_identificateur alias_identificateur_mi_unit_rom={0,0,"_mi",0,0};
  const identificateur & _IDNT_id_mi_unit_rom=* (const identificateur *) &alias_identificateur_mi_unit_rom;
  const alias_ref_identificateur ref_mi_unit_rom={-1,0,0,"_mi",0,0};
  const define_alias_gen(alias_mi_unit_rom,_IDNT,0,&ref_mi_unit_rom);
  const gen & mi_unit_rom_IDNT_e = * (gen *) & alias_mi_unit_rom;

  static const alias_identificateur alias_identificateur_miUS_unit_rom={0,0,"_miUS",0,0};
  const identificateur & _IDNT_id_miUS_unit_rom=* (const identificateur *) &alias_identificateur_miUS_unit_rom;
  const alias_ref_identificateur ref_miUS_unit_rom={-1,0,0,"_miUS",0,0};
  const define_alias_gen(alias_miUS_unit_rom,_IDNT,0,&ref_miUS_unit_rom);
  const gen & miUS_unit_rom_IDNT_e = * (gen *) & alias_miUS_unit_rom;

  static const alias_identificateur alias_identificateur_mil_unit_rom={0,0,"_mil",0,0};
  const identificateur & _IDNT_id_mil_unit_rom=* (const identificateur *) &alias_identificateur_mil_unit_rom;
  const alias_ref_identificateur ref_mil_unit_rom={-1,0,0,"_mil",0,0};
  const define_alias_gen(alias_mil_unit_rom,_IDNT,0,&ref_mil_unit_rom);
  const gen & mil_unit_rom_IDNT_e = * (gen *) & alias_mil_unit_rom;

  static const alias_identificateur alias_identificateur_mile_unit_rom={0,0,"_mile",0,0};
  const identificateur & _IDNT_id_mile_unit_rom=* (const identificateur *) &alias_identificateur_mile_unit_rom;
  const alias_ref_identificateur ref_mile_unit_rom={-1,0,0,"_mile",0,0};
  const define_alias_gen(alias_mile_unit_rom,_IDNT,0,&ref_mile_unit_rom);
  const gen & mile_unit_rom_IDNT_e = * (gen *) & alias_mile_unit_rom;

  static const alias_identificateur alias_identificateur_mille_unit_rom={0,0,"_mille",0,0};
  const identificateur & _IDNT_id_mille_unit_rom=* (const identificateur *) &alias_identificateur_mille_unit_rom;
  const alias_ref_identificateur ref_mille_unit_rom={-1,0,0,"_mille",0,0};
  const define_alias_gen(alias_mille_unit_rom,_IDNT,0,&ref_mille_unit_rom);
  const gen & mille_unit_rom_IDNT_e = * (gen *) & alias_mille_unit_rom;

  static const alias_identificateur alias_identificateur_min_unit_rom={0,0,"_min",0,0};
  const identificateur & _IDNT_id_min_unit_rom=* (const identificateur *) &alias_identificateur_min_unit_rom;
  const alias_ref_identificateur ref_min_unit_rom={-1,0,0,"_min",0,0};
  const define_alias_gen(alias_min_unit_rom,_IDNT,0,&ref_min_unit_rom);
  const gen & min_unit_rom_IDNT_e = * (gen *) & alias_min_unit_rom;

  static const alias_identificateur alias_identificateur_mmHg_unit_rom={0,0,"_mmHg",0,0};
  const identificateur & _IDNT_id_mmHg_unit_rom=* (const identificateur *) &alias_identificateur_mmHg_unit_rom;
  const alias_ref_identificateur ref_mmHg_unit_rom={-1,0,0,"_mmHg",0,0};
  const define_alias_gen(alias_mmHg_unit_rom,_IDNT,0,&ref_mmHg_unit_rom);
  const gen & mmHg_unit_rom_IDNT_e = * (gen *) & alias_mmHg_unit_rom;

  static const alias_identificateur alias_identificateur_mn_unit_rom={0,0,"_mn",0,0};
  const identificateur & _IDNT_id_mn_unit_rom=* (const identificateur *) &alias_identificateur_mn_unit_rom;
  const alias_ref_identificateur ref_mn_unit_rom={-1,0,0,"_mn",0,0};
  const define_alias_gen(alias_mn_unit_rom,_IDNT,0,&ref_mn_unit_rom);
  const gen & mn_unit_rom_IDNT_e = * (gen *) & alias_mn_unit_rom;

  static const alias_identificateur alias_identificateur_mol_unit_rom={0,0,"_mol",0,0};
  const identificateur & _IDNT_id_mol_unit_rom=* (const identificateur *) &alias_identificateur_mol_unit_rom;
  const alias_ref_identificateur ref_mol_unit_rom={-1,0,0,"_mol",0,0};
  const define_alias_gen(alias_mol_unit_rom,_IDNT,0,&ref_mol_unit_rom);
  const gen & mol_unit_rom_IDNT_e = * (gen *) & alias_mol_unit_rom;

  static const alias_identificateur alias_identificateur_molK_unit_rom={0,0,"_molK",0,0};
  const identificateur & _IDNT_id_molK_unit_rom=* (const identificateur *) &alias_identificateur_molK_unit_rom;
  const alias_ref_identificateur ref_molK_unit_rom={-1,0,0,"_molK",0,0};
  const define_alias_gen(alias_molK_unit_rom,_IDNT,0,&ref_molK_unit_rom);
  const gen & molK_unit_rom_IDNT_e = * (gen *) & alias_molK_unit_rom;

  static const alias_identificateur alias_identificateur_mp__unit_rom={0,0,"_mp_",0,0};
  const identificateur & _IDNT_id_mp__unit_rom=* (const identificateur *) &alias_identificateur_mp__unit_rom;
  const alias_ref_identificateur ref_mp__unit_rom={-1,0,0,"_mp_",0,0};
  const define_alias_gen(alias_mp__unit_rom,_IDNT,0,&ref_mp__unit_rom);
  const gen & mp__unit_rom_IDNT_e = * (gen *) & alias_mp__unit_rom;

  static const alias_identificateur alias_identificateur_mph_unit_rom={0,0,"_mph",0,0};
  const identificateur & _IDNT_id_mph_unit_rom=* (const identificateur *) &alias_identificateur_mph_unit_rom;
  const alias_ref_identificateur ref_mph_unit_rom={-1,0,0,"_mph",0,0};
  const define_alias_gen(alias_mph_unit_rom,_IDNT,0,&ref_mph_unit_rom);
  const gen & mph_unit_rom_IDNT_e = * (gen *) & alias_mph_unit_rom;

  static const alias_identificateur alias_identificateur_mpme__unit_rom={0,0,"_mpme_",0,0};
  const identificateur & _IDNT_id_mpme__unit_rom=* (const identificateur *) &alias_identificateur_mpme__unit_rom;
  const alias_ref_identificateur ref_mpme__unit_rom={-1,0,0,"_mpme",0,0};
  const define_alias_gen(alias_mpme__unit_rom,_IDNT,0,&ref_mpme__unit_rom);
  const gen & mpme__unit_rom_IDNT_e = * (gen *) & alias_mpme__unit_rom;

  static const alias_identificateur alias_identificateur_mu0__unit_rom={0,0,"_mu0_",0,0};
  const identificateur & _IDNT_id_mu0__unit_rom=* (const identificateur *) &alias_identificateur_mu0__unit_rom;
  const alias_ref_identificateur ref_mu0__unit_rom={-1,0,0,"_mu0_",0,0};
  const define_alias_gen(alias_mu0__unit_rom,_IDNT,0,&ref_mu0__unit_rom);
  const gen & mu0__unit_rom_IDNT_e = * (gen *) & alias_mu0__unit_rom;

  static const alias_identificateur alias_identificateur_muB__unit_rom={0,0,"_muB_",0,0};
  const identificateur & _IDNT_id_muB__unit_rom=* (const identificateur *) &alias_identificateur_muB__unit_rom;
  const alias_ref_identificateur ref_muB__unit_rom={-1,0,0,"_muB_",0,0};
  const define_alias_gen(alias_muB__unit_rom,_IDNT,0,&ref_muB__unit_rom);
  const gen & muB__unit_rom_IDNT_e = * (gen *) & alias_muB__unit_rom;

  static const alias_identificateur alias_identificateur_muN__unit_rom={0,0,"_muN_",0,0};
  const identificateur & _IDNT_id_muN__unit_rom=* (const identificateur *) &alias_identificateur_muN__unit_rom;
  const alias_ref_identificateur ref_muN__unit_rom={-1,0,0,"_muN_",0,0};
  const define_alias_gen(alias_muN__unit_rom,_IDNT,0,&ref_muN__unit_rom);
  const gen & muN__unit_rom_IDNT_e = * (gen *) & alias_muN__unit_rom;

  static const alias_identificateur alias_identificateur_nmi_unit_rom={0,0,"_nmi",0,0};
  const identificateur & _IDNT_id_nmi_unit_rom=* (const identificateur *) &alias_identificateur_nmi_unit_rom;
  const alias_ref_identificateur ref_nmi_unit_rom={-1,0,0,"_nmi",0,0};
  const define_alias_gen(alias_nmi_unit_rom,_IDNT,0,&ref_nmi_unit_rom);
  const gen & nmi_unit_rom_IDNT_e = * (gen *) & alias_nmi_unit_rom;

  static const alias_identificateur alias_identificateur_oz_unit_rom={0,0,"_oz",0,0};
  const identificateur & _IDNT_id_oz_unit_rom=* (const identificateur *) &alias_identificateur_oz_unit_rom;
  const alias_ref_identificateur ref_oz_unit_rom={-1,0,0,"_oz",0,0};
  const define_alias_gen(alias_oz_unit_rom,_IDNT,0,&ref_oz_unit_rom);
  const gen & oz_unit_rom_IDNT_e = * (gen *) & alias_oz_unit_rom;

  static const alias_identificateur alias_identificateur_ozUK_unit_rom={0,0,"_ozUK",0,0};
  const identificateur & _IDNT_id_ozUK_unit_rom=* (const identificateur *) &alias_identificateur_ozUK_unit_rom;
  const alias_ref_identificateur ref_ozUK_unit_rom={-1,0,0,"_ozUK",0,0};
  const define_alias_gen(alias_ozUK_unit_rom,_IDNT,0,&ref_ozUK_unit_rom);
  const gen & ozUK_unit_rom_IDNT_e = * (gen *) & alias_ozUK_unit_rom;

  static const alias_identificateur alias_identificateur_ozfl_unit_rom={0,0,"_ozfl",0,0};
  const identificateur & _IDNT_id_ozfl_unit_rom=* (const identificateur *) &alias_identificateur_ozfl_unit_rom;
  const alias_ref_identificateur ref_ozfl_unit_rom={-1,0,0,"_ozfl",0,0};
  const define_alias_gen(alias_ozfl_unit_rom,_IDNT,0,&ref_ozfl_unit_rom);
  const gen & ozfl_unit_rom_IDNT_e = * (gen *) & alias_ozfl_unit_rom;

  static const alias_identificateur alias_identificateur_ozt_unit_rom={0,0,"_ozt",0,0};
  const identificateur & _IDNT_id_ozt_unit_rom=* (const identificateur *) &alias_identificateur_ozt_unit_rom;
  const alias_ref_identificateur ref_ozt_unit_rom={-1,0,0,"_ozt",0,0};
  const define_alias_gen(alias_ozt_unit_rom,_IDNT,0,&ref_ozt_unit_rom);
  const gen & ozt_unit_rom_IDNT_e = * (gen *) & alias_ozt_unit_rom;

  static const alias_identificateur alias_identificateur_pc_unit_rom={0,0,"_pc",0,0};
  const identificateur & _IDNT_id_pc_unit_rom=* (const identificateur *) &alias_identificateur_pc_unit_rom;
  const alias_ref_identificateur ref_pc_unit_rom={-1,0,0,"_pc",0,0};
  const define_alias_gen(alias_pc_unit_rom,_IDNT,0,&ref_pc_unit_rom);
  const gen & pc_unit_rom_IDNT_e = * (gen *) & alias_pc_unit_rom;

  static const alias_identificateur alias_identificateur_pdl_unit_rom={0,0,"_pdl",0,0};
  const identificateur & _IDNT_id_pdl_unit_rom=* (const identificateur *) &alias_identificateur_pdl_unit_rom;
  const alias_ref_identificateur ref_pdl_unit_rom={-1,0,0,"_pdl",0,0};
  const define_alias_gen(alias_pdl_unit_rom,_IDNT,0,&ref_pdl_unit_rom);
  const gen & pdl_unit_rom_IDNT_e = * (gen *) & alias_pdl_unit_rom;

  static const alias_identificateur alias_identificateur_phi__unit_rom={0,0,"_phi_",0,0};
  const identificateur & _IDNT_id_phi__unit_rom=* (const identificateur *) &alias_identificateur_phi__unit_rom;
  const alias_ref_identificateur ref_phi__unit_rom={-1,0,0,"_phi_",0,0};
  const define_alias_gen(alias_phi__unit_rom,_IDNT,0,&ref_phi__unit_rom);
  const gen & phi__unit_rom_IDNT_e = * (gen *) & alias_phi__unit_rom;

  static const alias_identificateur alias_identificateur_pk_unit_rom={0,0,"_pk",0,0};
  const identificateur & _IDNT_id_pk_unit_rom=* (const identificateur *) &alias_identificateur_pk_unit_rom;
  const alias_ref_identificateur ref_pk_unit_rom={-1,0,0,"_pk",0,0};
  const define_alias_gen(alias_pk_unit_rom,_IDNT,0,&ref_pk_unit_rom);
  const gen & pk_unit_rom_IDNT_e = * (gen *) & alias_pk_unit_rom;

  static const alias_identificateur alias_identificateur_psi_unit_rom={0,0,"_psi",0,0};
  const identificateur & _IDNT_id_psi_unit_rom=* (const identificateur *) &alias_identificateur_psi_unit_rom;
  const alias_ref_identificateur ref_psi_unit_rom={-1,0,0,"_psi",0,0};
  const define_alias_gen(alias_psi_unit_rom,_IDNT,0,&ref_psi_unit_rom);
  const gen & psi_unit_rom_IDNT_e = * (gen *) & alias_psi_unit_rom;

  static const alias_identificateur alias_identificateur_pt_unit_rom={0,0,"_pt",0,0};
  const identificateur & _IDNT_id_pt_unit_rom=* (const identificateur *) &alias_identificateur_pt_unit_rom;
  const alias_ref_identificateur ref_pt_unit_rom={-1,0,0,"_pt",0,0};
  const define_alias_gen(alias_pt_unit_rom,_IDNT,0,&ref_pt_unit_rom);
  const gen & pt_unit_rom_IDNT_e = * (gen *) & alias_pt_unit_rom;

  static const alias_identificateur alias_identificateur_ptUK_unit_rom={0,0,"_ptUK",0,0};
  const identificateur & _IDNT_id_ptUK_unit_rom=* (const identificateur *) &alias_identificateur_ptUK_unit_rom;
  const alias_ref_identificateur ref_ptUK_unit_rom={-1,0,0,"_ptUK",0,0};
  const define_alias_gen(alias_ptUK_unit_rom,_IDNT,0,&ref_ptUK_unit_rom);
  const gen & ptUK_unit_rom_IDNT_e = * (gen *) & alias_ptUK_unit_rom;

  static const alias_identificateur alias_identificateur_qe_unit_rom={0,0,"_qe",0,0};
  const identificateur & _IDNT_id_qe_unit_rom=* (const identificateur *) &alias_identificateur_qe_unit_rom;
  const alias_ref_identificateur ref_qe_unit_rom={-1,0,0,"_qe",0,0};
  const define_alias_gen(alias_qe_unit_rom,_IDNT,0,&ref_qe_unit_rom);
  const gen & qe_unit_rom_IDNT_e = * (gen *) & alias_qe_unit_rom;

  static const alias_identificateur alias_identificateur_qepsilon0__unit_rom={0,0,"_qepsilon0_",0,0};
  const identificateur & _IDNT_id_qepsilon0__unit_rom=* (const identificateur *) &alias_identificateur_qepsilon0__unit_rom;
  const alias_ref_identificateur ref_qepsilon0__unit_rom={-1,0,0,"_qepsilon0_",0,0};
  const define_alias_gen(alias_qepsilon0__unit_rom,_IDNT,0,&ref_qepsilon0__unit_rom);
  const gen & qepsilon0__unit_rom_IDNT_e = * (gen *) & alias_qepsilon0__unit_rom;

  static const alias_identificateur alias_identificateur_qme__unit_rom={0,0,"_qme_",0,0};
  const identificateur & _IDNT_id_qme__unit_rom=* (const identificateur *) &alias_identificateur_qme__unit_rom;
  const alias_ref_identificateur ref_qme__unit_rom={-1,0,0,"_qme_",0,0};
  const define_alias_gen(alias_qme__unit_rom,_IDNT,0,&ref_qme__unit_rom);
  const gen & qme__unit_rom_IDNT_e = * (gen *) & alias_qme__unit_rom;

  static const alias_identificateur alias_identificateur_qt_unit_rom={0,0,"_qt",0,0};
  const identificateur & _IDNT_id_qt_unit_rom=* (const identificateur *) &alias_identificateur_qt_unit_rom;
  const alias_ref_identificateur ref_qt_unit_rom={-1,0,0,"_qt",0,0};
  const define_alias_gen(alias_qt_unit_rom,_IDNT,0,&ref_qt_unit_rom);
  const gen & qt_unit_rom_IDNT_e = * (gen *) & alias_qt_unit_rom;

  static const alias_identificateur alias_identificateur_rad_unit_rom={0,0,"_rad",0,0};
  const identificateur & _IDNT_id_rad_unit_rom=* (const identificateur *) &alias_identificateur_rad_unit_rom;
  const alias_ref_identificateur ref_rad_unit_rom={-1,0,0,"_rad",0,0};
  const define_alias_gen(alias_rad_unit_rom,_IDNT,0,&ref_rad_unit_rom);
  const gen & rad_unit_rom_IDNT_e = * (gen *) & alias_rad_unit_rom;

  static const alias_identificateur alias_identificateur_rd_unit_rom={0,0,"_rd",0,0};
  const identificateur & _IDNT_id_rd_unit_rom=* (const identificateur *) &alias_identificateur_rd_unit_rom;
  const alias_ref_identificateur ref_rd_unit_rom={-1,0,0,"_rd",0,0};
  const define_alias_gen(alias_rd_unit_rom,_IDNT,0,&ref_rd_unit_rom);
  const gen & rd_unit_rom_IDNT_e = * (gen *) & alias_rd_unit_rom;

  static const alias_identificateur alias_identificateur_rem_unit_rom={0,0,"_rem",0,0};
  const identificateur & _IDNT_id_rem_unit_rom=* (const identificateur *) &alias_identificateur_rem_unit_rom;
  const alias_ref_identificateur ref_rem_unit_rom={-1,0,0,"_rem",0,0};
  const define_alias_gen(alias_rem_unit_rom,_IDNT,0,&ref_rem_unit_rom);
  const gen & rem_unit_rom_IDNT_e = * (gen *) & alias_rem_unit_rom;

  static const alias_identificateur alias_identificateur_rev_unit_rom={0,0,"_rev",0,0};
  const identificateur & _IDNT_id_rev_unit_rom=* (const identificateur *) &alias_identificateur_rev_unit_rom;
  const alias_ref_identificateur ref_rev_unit_rom={-1,0,0,"_rev",0,0};
  const define_alias_gen(alias_rev_unit_rom,_IDNT,0,&ref_rev_unit_rom);
  const gen & rev_unit_rom_IDNT_e = * (gen *) & alias_rev_unit_rom;

  static const alias_identificateur alias_identificateur_rod_unit_rom={0,0,"_rod",0,0};
  const identificateur & _IDNT_id_rod_unit_rom=* (const identificateur *) &alias_identificateur_rod_unit_rom;
  const alias_ref_identificateur ref_rod_unit_rom={-1,0,0,"_rod",0,0};
  const define_alias_gen(alias_rod_unit_rom,_IDNT,0,&ref_rod_unit_rom);
  const gen & rod_unit_rom_IDNT_e = * (gen *) & alias_rod_unit_rom;

  static const alias_identificateur alias_identificateur_rpm_unit_rom={0,0,"_rpm",0,0};
  const identificateur & _IDNT_id_rpm_unit_rom=* (const identificateur *) &alias_identificateur_rpm_unit_rom;
  const alias_ref_identificateur ref_rpm_unit_rom={-1,0,0,"_rpm",0,0};
  const define_alias_gen(alias_rpm_unit_rom,_IDNT,0,&ref_rpm_unit_rom);
  const gen & rpm_unit_rom_IDNT_e = * (gen *) & alias_rpm_unit_rom;

  static const alias_identificateur alias_identificateur_s_unit_rom={0,0,"_s",0,0};
  const identificateur & _IDNT_id_s_unit_rom=* (const identificateur *) &alias_identificateur_s_unit_rom;
  const alias_ref_identificateur ref_s_unit_rom={-1,0,0,"_s",0,0};
  const define_alias_gen(alias_s_unit_rom,_IDNT,0,&ref_s_unit_rom);
  const gen & s_unit_rom_IDNT_e = * (gen *) & alias_s_unit_rom;

  static const alias_identificateur alias_identificateur_sb_unit_rom={0,0,"_sb",0,0};
  const identificateur & _IDNT_id_sb_unit_rom=* (const identificateur *) &alias_identificateur_sb_unit_rom;
  const alias_ref_identificateur ref_sb_unit_rom={-1,0,0,"_sb",0,0};
  const define_alias_gen(alias_sb_unit_rom,_IDNT,0,&ref_sb_unit_rom);
  const gen & sb_unit_rom_IDNT_e = * (gen *) & alias_sb_unit_rom;

  static const alias_identificateur alias_identificateur_sd_unit_rom={0,0,"_sd",0,0};
  const identificateur & _IDNT_id_sd_unit_rom=* (const identificateur *) &alias_identificateur_sd_unit_rom;
  const alias_ref_identificateur ref_sd_unit_rom={-1,0,0,"_sd",0,0};
  const define_alias_gen(alias_sd_unit_rom,_IDNT,0,&ref_sd_unit_rom);
  const gen & sd_unit_rom_IDNT_e = * (gen *) & alias_sd_unit_rom;

  static const alias_identificateur alias_identificateur_sigma_unit_rom={0,0,"_sigma",0,0};
  const identificateur & _IDNT_id_sigma_unit_rom=* (const identificateur *) &alias_identificateur_sigma_unit_rom;
  const alias_ref_identificateur ref_sigma_unit_rom={-1,0,0,"_sigma",0,0};
  const define_alias_gen(alias_sigma_unit_rom,_IDNT,0,&ref_sigma_unit_rom);
  const gen & sigma_unit_rom_IDNT_e = * (gen *) & alias_sigma_unit_rom;

  static const alias_identificateur alias_identificateur_slug_unit_rom={0,0,"_slug",0,0};
  const identificateur & _IDNT_id_slug_unit_rom=* (const identificateur *) &alias_identificateur_slug_unit_rom;
  const alias_ref_identificateur ref_slug_unit_rom={-1,0,0,"_slug",0,0};
  const define_alias_gen(alias_slug_unit_rom,_IDNT,0,&ref_slug_unit_rom);
  const gen & slug_unit_rom_IDNT_e = * (gen *) & alias_slug_unit_rom;

  static const alias_identificateur alias_identificateur_st_unit_rom={0,0,"_st",0,0};
  const identificateur & _IDNT_id_st_unit_rom=* (const identificateur *) &alias_identificateur_st_unit_rom;
  const alias_ref_identificateur ref_st_unit_rom={-1,0,0,"_st",0,0};
  const define_alias_gen(alias_st_unit_rom,_IDNT,0,&ref_st_unit_rom);
  const gen & st_unit_rom_IDNT_e = * (gen *) & alias_st_unit_rom;

  static const alias_identificateur alias_identificateur_syr__unit_rom={0,0,"_syr_",0,0};
  const identificateur & _IDNT_id_syr__unit_rom=* (const identificateur *) &alias_identificateur_syr__unit_rom;
  const alias_ref_identificateur ref_syr__unit_rom={-1,0,0,"_syr_",0,0};
  const define_alias_gen(alias_syr__unit_rom,_IDNT,0,&ref_syr__unit_rom);
  const gen & syr__unit_rom_IDNT_e = * (gen *) & alias_syr__unit_rom;

  static const alias_identificateur alias_identificateur_t_unit_rom={0,0,"_t",0,0};
  const identificateur & _IDNT_id_t_unit_rom=* (const identificateur *) &alias_identificateur_t_unit_rom;
  const alias_ref_identificateur ref_t_unit_rom={-1,0,0,"_t",0,0};
  const define_alias_gen(alias_t_unit_rom,_IDNT,0,&ref_t_unit_rom);
  const gen & t_unit_rom_IDNT_e = * (gen *) & alias_t_unit_rom;

  static const alias_identificateur alias_identificateur_tbsp_unit_rom={0,0,"_tbsp",0,0};
  const identificateur & _IDNT_id_tbsp_unit_rom=* (const identificateur *) &alias_identificateur_tbsp_unit_rom;
  const alias_ref_identificateur ref_tbsp_unit_rom={-1,0,0,"_tbsp",0,0};
  const define_alias_gen(alias_tbsp_unit_rom,_IDNT,0,&ref_tbsp_unit_rom);
  const gen & tbsp_unit_rom_IDNT_e = * (gen *) & alias_tbsp_unit_rom;

  static const alias_identificateur alias_identificateur_tec_unit_rom={0,0,"_tec",0,0};
  const identificateur & _IDNT_id_tec_unit_rom=* (const identificateur *) &alias_identificateur_tec_unit_rom;
  const alias_ref_identificateur ref_tec_unit_rom={-1,0,0,"_tec",0,0};
  const define_alias_gen(alias_tec_unit_rom,_IDNT,0,&ref_tec_unit_rom);
  const gen & tec_unit_rom_IDNT_e = * (gen *) & alias_tec_unit_rom;

  static const alias_identificateur alias_identificateur_tep_unit_rom={0,0,"_tep",0,0};
  const identificateur & _IDNT_id_tep_unit_rom=* (const identificateur *) &alias_identificateur_tep_unit_rom;
  const alias_ref_identificateur ref_tep_unit_rom={-1,0,0,"_tep",0,0};
  const define_alias_gen(alias_tep_unit_rom,_IDNT,0,&ref_tep_unit_rom);
  const gen & tep_unit_rom_IDNT_e = * (gen *) & alias_tep_unit_rom;

  static const alias_identificateur alias_identificateur_tepC_unit_rom={0,0,"_tepC",0,0};
  const identificateur & _IDNT_id_tepC_unit_rom=* (const identificateur *) &alias_identificateur_tepC_unit_rom;
  const alias_ref_identificateur ref_tepC_unit_rom={-1,0,0,"_tepC",0,0};
  const define_alias_gen(alias_tepC_unit_rom,_IDNT,0,&ref_tepC_unit_rom);
  const gen & tepC_unit_rom_IDNT_e = * (gen *) & alias_tepC_unit_rom;

  static const alias_identificateur alias_identificateur_tepcC_unit_rom={0,0,"_tepcC",0,0};
  const identificateur & _IDNT_id_tepcC_unit_rom=* (const identificateur *) &alias_identificateur_tepcC_unit_rom;
  const alias_ref_identificateur ref_tepcC_unit_rom={-1,0,0,"_tepcC",0,0};
  const define_alias_gen(alias_tepcC_unit_rom,_IDNT,0,&ref_tepcC_unit_rom);
  const gen & tepcC_unit_rom_IDNT_e = * (gen *) & alias_tepcC_unit_rom;

  static const alias_identificateur alias_identificateur_tepgC_unit_rom={0,0,"_tepgC",0,0};
  const identificateur & _IDNT_id_tepgC_unit_rom=* (const identificateur *) &alias_identificateur_tepgC_unit_rom;
  const alias_ref_identificateur ref_tepgC_unit_rom={-1,0,0,"_tepgC",0,0};
  const define_alias_gen(alias_tepgC_unit_rom,_IDNT,0,&ref_tepgC_unit_rom);
  const gen & tepgC_unit_rom_IDNT_e = * (gen *) & alias_tepgC_unit_rom;

  static const alias_identificateur alias_identificateur_tex_unit_rom={0,0,"_tex",0,0};
  const identificateur & _IDNT_id_tex_unit_rom=* (const identificateur *) &alias_identificateur_tex_unit_rom;
  const alias_ref_identificateur ref_tex_unit_rom={-1,0,0,"_tex",0,0};
  const define_alias_gen(alias_tex_unit_rom,_IDNT,0,&ref_tex_unit_rom);
  const gen & tex_unit_rom_IDNT_e = * (gen *) & alias_tex_unit_rom;

  static const alias_identificateur alias_identificateur_therm_unit_rom={0,0,"_therm",0,0};
  const identificateur & _IDNT_id_therm_unit_rom=* (const identificateur *) &alias_identificateur_therm_unit_rom;
  const alias_ref_identificateur ref_therm_unit_rom={-1,0,0,"_therm",0,0};
  const define_alias_gen(alias_therm_unit_rom,_IDNT,0,&ref_therm_unit_rom);
  const gen & therm_unit_rom_IDNT_e = * (gen *) & alias_therm_unit_rom;

  static const alias_identificateur alias_identificateur_toe_unit_rom={0,0,"_toe",0,0};
  const identificateur & _IDNT_id_toe_unit_rom=* (const identificateur *) &alias_identificateur_toe_unit_rom;
  const alias_ref_identificateur ref_toe_unit_rom={-1,0,0,"_toe",0,0};
  const define_alias_gen(alias_toe_unit_rom,_IDNT,0,&ref_toe_unit_rom);
  const gen & toe_unit_rom_IDNT_e = * (gen *) & alias_toe_unit_rom;

  static const alias_identificateur alias_identificateur_ton_unit_rom={0,0,"_ton",0,0};
  const identificateur & _IDNT_id_ton_unit_rom=* (const identificateur *) &alias_identificateur_ton_unit_rom;
  const alias_ref_identificateur ref_ton_unit_rom={-1,0,0,"_ton",0,0};
  const define_alias_gen(alias_ton_unit_rom,_IDNT,0,&ref_ton_unit_rom);
  const gen & ton_unit_rom_IDNT_e = * (gen *) & alias_ton_unit_rom;

  static const alias_identificateur alias_identificateur_tonUK_unit_rom={0,0,"_tonUK",0,0};
  const identificateur & _IDNT_id_tonUK_unit_rom=* (const identificateur *) &alias_identificateur_tonUK_unit_rom;
  const alias_ref_identificateur ref_tonUK_unit_rom={-1,0,0,"_tonUK",0,0};
  const define_alias_gen(alias_tonUK_unit_rom,_IDNT,0,&ref_tonUK_unit_rom);
  const gen & tonUK_unit_rom_IDNT_e = * (gen *) & alias_tonUK_unit_rom;

  static const alias_identificateur alias_identificateur_tr_unit_rom={0,0,"_tr",0,0};
  const identificateur & _IDNT_id_tr_unit_rom=* (const identificateur *) &alias_identificateur_tr_unit_rom;
  const alias_ref_identificateur ref_tr_unit_rom={-1,0,0,"_tr",0,0};
  const define_alias_gen(alias_tr_unit_rom,_IDNT,0,&ref_tr_unit_rom);
  const gen & tr_unit_rom_IDNT_e = * (gen *) & alias_tr_unit_rom;

  static const alias_identificateur alias_identificateur_tsp_unit_rom={0,0,"_tsp",0,0};
  const identificateur & _IDNT_id_tsp_unit_rom=* (const identificateur *) &alias_identificateur_tsp_unit_rom;
  const alias_ref_identificateur ref_tsp_unit_rom={-1,0,0,"_tsp",0,0};
  const define_alias_gen(alias_tsp_unit_rom,_IDNT,0,&ref_tsp_unit_rom);
  const gen & tsp_unit_rom_IDNT_e = * (gen *) & alias_tsp_unit_rom;

  static const alias_identificateur alias_identificateur_u_unit_rom={0,0,"_u",0,0};
  const identificateur & _IDNT_id_u_unit_rom=* (const identificateur *) &alias_identificateur_u_unit_rom;
  const alias_ref_identificateur ref_u_unit_rom={-1,0,0,"_u",0,0};
  const define_alias_gen(alias_u_unit_rom,_IDNT,0,&ref_u_unit_rom);
  const gen & u_unit_rom_IDNT_e = * (gen *) & alias_u_unit_rom;

  static const alias_identificateur alias_identificateur_E_unit_rom={0,0,"_E",0,0};
  const identificateur & _IDNT_id_E_unit_rom=* (const identificateur *) &alias_identificateur_E_unit_rom;
  const alias_ref_identificateur ref_E_unit_rom={-1,0,0,"_E",0,0};
  const define_alias_gen(alias_E_unit_rom,_IDNT,0,&ref_E_unit_rom);
  const gen & E_unit_rom_IDNT_e = * (gen *) & alias_E_unit_rom;

  static const alias_identificateur alias_identificateur_Curie_unit_rom={0,0,"_Curie",0,0};
  const identificateur & _IDNT_id_Curie_unit_rom=* (const identificateur *) &alias_identificateur_Curie_unit_rom;
  const alias_ref_identificateur ref_Curie_unit_rom={-1,0,0,"_Curie",0,0};
  const define_alias_gen(alias_Curie_unit_rom,_IDNT,0,&ref_Curie_unit_rom);
  const gen & Curie_unit_rom_IDNT_e = * (gen *) & alias_Curie_unit_rom;

  static const alias_identificateur alias_identificateur_C_unit_rom={0,0,"_C",0,0};
  const identificateur & _IDNT_id_C_unit_rom=* (const identificateur *) &alias_identificateur_C_unit_rom;
  const alias_ref_identificateur ref_C_unit_rom={-1,0,0,"_C",0,0};
  const define_alias_gen(alias_C_unit_rom,_IDNT,0,&ref_C_unit_rom);
  const gen & C_unit_rom_IDNT_e = * (gen *) & alias_C_unit_rom;

  static const alias_identificateur alias_identificateur_kg_unit_rom={0,0,"_kg",0,0};
  const identificateur & _IDNT_id_kg_unit_rom=* (const identificateur *) &alias_identificateur_kg_unit_rom;
  const alias_ref_identificateur ref_kg_unit_rom={-1,0,0,"_kg",0,0};
  const define_alias_gen(alias_kg_unit_rom,_IDNT,0,&ref_kg_unit_rom);
  const gen & kg_unit_rom_IDNT_e = * (gen *) & alias_kg_unit_rom;
  // const gen & _kg_unit = * (gen *) & alias_kg_unit_rom;


#else
  identificateur m_unit_rom_IDNT("_m");
  gen m_unit_rom_IDNT_e(m_unit_rom_IDNT);
  identificateur A_unit_rom_IDNT("_A");
  gen A_unit_rom_IDNT_e(A_unit_rom_IDNT);
  identificateur Bq_unit_rom_IDNT("_Bq");
  gen Bq_unit_rom_IDNT_e(Bq_unit_rom_IDNT);
  identificateur yd_unit_rom_IDNT("_yd");
  gen yd_unit_rom_IDNT_e(yd_unit_rom_IDNT);
  identificateur yr_unit_rom_IDNT("_yr");
  gen yr_unit_rom_IDNT_e(yr_unit_rom_IDNT);
  identificateur Btu_unit_rom_IDNT("_Btu");
  gen Btu_unit_rom_IDNT_e(Btu_unit_rom_IDNT);
  identificateur R__unit_rom_IDNT("_R_");
  gen R__unit_rom_IDNT_e(R__unit_rom_IDNT);
  identificateur epsilon0__unit_rom_IDNT("_epsilon0_");
  gen epsilon0__unit_rom_IDNT_e(epsilon0__unit_rom_IDNT);
  identificateur a0__unit_rom_IDNT("_a0_");
  gen a0__unit_rom_IDNT_e(a0__unit_rom_IDNT);
  identificateur RSun__unit_rom_IDNT("_RSun_");
  gen RSun__unit_rom_IDNT_e(RSun__unit_rom_IDNT);
  identificateur REarth__unit_rom_IDNT("_REarth_");
  gen REarth__unit_rom_IDNT_e(REarth__unit_rom_IDNT);
  identificateur Fdy_unit_rom_IDNT("_Fdy");
  gen Fdy_unit_rom_IDNT_e(Fdy_unit_rom_IDNT);
  identificateur F_unit_rom_IDNT("_F");
  gen F_unit_rom_IDNT_e(F_unit_rom_IDNT);
  identificateur FF_unit_rom_IDNT("_FF");
  gen FF_unit_rom_IDNT_e(FF_unit_rom_IDNT);
  identificateur Faraday__unit_rom_IDNT("_F_");
  gen Faraday__unit_rom_IDNT_e(Faraday__unit_rom_IDNT);
  identificateur G__unit_rom_IDNT("_G_");
  gen G__unit_rom_IDNT_e(G__unit_rom_IDNT);
  identificateur Gal_unit_rom_IDNT("_Gal");
  gen Gal_unit_rom_IDNT_e(Gal_unit_rom_IDNT);
  identificateur Gy_unit_rom_IDNT("_Gy");
  gen Gy_unit_rom_IDNT_e(Gy_unit_rom_IDNT);
  identificateur H_unit_rom_IDNT("_H");
  gen H_unit_rom_IDNT_e(H_unit_rom_IDNT);
  identificateur HFCC_unit_rom_IDNT("_HFCC");
  gen HFCC_unit_rom_IDNT_e(HFCC_unit_rom_IDNT);
  identificateur Hz_unit_rom_IDNT("_Hz");
  gen Hz_unit_rom_IDNT_e(Hz_unit_rom_IDNT);
  identificateur IO__unit_rom_IDNT("_IO_");
  gen IO__unit_rom_IDNT_e(IO__unit_rom_IDNT);
  identificateur J_unit_rom_IDNT("_J");
  gen J_unit_rom_IDNT_e(J_unit_rom_IDNT);
  identificateur K_unit_rom_IDNT("_K");
  gen K_unit_rom_IDNT_e(K_unit_rom_IDNT);
  identificateur L_unit_rom_IDNT("_L");
  gen L_unit_rom_IDNT_e(L_unit_rom_IDNT);
  identificateur N_unit_rom_IDNT("_N");
  gen N_unit_rom_IDNT_e(N_unit_rom_IDNT);
  identificateur NA__unit_rom_IDNT("_NA_");
  gen NA__unit_rom_IDNT_e(NA__unit_rom_IDNT);
  identificateur Ohm_unit_rom_IDNT("_Ohm");
  gen Ohm_unit_rom_IDNT_e(Ohm_unit_rom_IDNT);
  identificateur P_unit_rom_IDNT("_P");
  gen P_unit_rom_IDNT_e(P_unit_rom_IDNT);
  identificateur PSun__unit_rom_IDNT("_PSun_");
  gen PSun__unit_rom_IDNT_e(PSun__unit_rom_IDNT);
  identificateur Pa_unit_rom_IDNT("_Pa");
  gen Pa_unit_rom_IDNT_e(Pa_unit_rom_IDNT);
  identificateur R_unit_rom_IDNT("_R");
  gen R_unit_rom_IDNT_e(R_unit_rom_IDNT);
  identificateur Rankine_unit_rom_IDNT("_Rankine");
  gen Rankine_unit_rom_IDNT_e(Rankine_unit_rom_IDNT);
  identificateur Rinfinity__unit_rom_IDNT("_Rinfinity_");
  gen Rinfinity__unit_rom_IDNT_e(Rinfinity__unit_rom_IDNT);
  identificateur S_unit_rom_IDNT("_S");
  gen S_unit_rom_IDNT_e(S_unit_rom_IDNT);
  identificateur St_unit_rom_IDNT("_St");
  gen St_unit_rom_IDNT_e(St_unit_rom_IDNT);
  identificateur StdP__unit_rom_IDNT("_StdP_");
  gen StdP__unit_rom_IDNT_e(StdP__unit_rom_IDNT);
  identificateur StdT__unit_rom_IDNT("_StdT_");
  gen StdT__unit_rom_IDNT_e(StdT__unit_rom_IDNT);
  identificateur Sv_unit_rom_IDNT("_Sv");
  gen Sv_unit_rom_IDNT_e(Sv_unit_rom_IDNT);
  identificateur T_unit_rom_IDNT("_T");
  gen T_unit_rom_IDNT_e(T_unit_rom_IDNT);
  identificateur Torr_unit_rom_IDNT("_Torr");
  gen Torr_unit_rom_IDNT_e(Torr_unit_rom_IDNT);
  identificateur V_unit_rom_IDNT("_V");
  gen V_unit_rom_IDNT_e(V_unit_rom_IDNT);
  identificateur Vm__unit_rom_IDNT("_Vm_");
  gen Vm__unit_rom_IDNT_e(Vm__unit_rom_IDNT);
  identificateur W_unit_rom_IDNT("_W");
  gen W_unit_rom_IDNT_e(W_unit_rom_IDNT);
  identificateur Wb_unit_rom_IDNT("_Wb");
  gen Wb_unit_rom_IDNT_e(Wb_unit_rom_IDNT);
  identificateur Wh_unit_rom_IDNT("_Wh");
  gen Wh_unit_rom_IDNT_e(Wh_unit_rom_IDNT);
  identificateur a_unit_rom_IDNT("_a");
  gen a_unit_rom_IDNT_e(a_unit_rom_IDNT);
  identificateur acre_unit_rom_IDNT("_acre");
  gen acre_unit_rom_IDNT_e(acre_unit_rom_IDNT);
  identificateur alpha__unit_rom_IDNT("_alpha_");
  gen alpha__unit_rom_IDNT_e(alpha__unit_rom_IDNT);
  identificateur arcmin_unit_rom_IDNT("_arcmin");
  gen arcmin_unit_rom_IDNT_e(arcmin_unit_rom_IDNT);
  identificateur arcs_unit_rom_IDNT("_arcs");
  gen arcs_unit_rom_IDNT_e(arcs_unit_rom_IDNT);
  identificateur atm_unit_rom_IDNT("_atm");
  gen atm_unit_rom_IDNT_e(atm_unit_rom_IDNT);
  identificateur au_unit_rom_IDNT("_au");
  gen au_unit_rom_IDNT_e(au_unit_rom_IDNT);
  identificateur bar_unit_rom_IDNT("_bar");
  gen bar_unit_rom_IDNT_e(bar_unit_rom_IDNT);
  identificateur b_unit_rom_IDNT("_b");
  gen b_unit_rom_IDNT_e(b_unit_rom_IDNT);
  identificateur bbl_unit_rom_IDNT("_bbl");
  gen bbl_unit_rom_IDNT_e(bbl_unit_rom_IDNT);
  identificateur bblep_unit_rom_IDNT("_bblep");
  gen bblep_unit_rom_IDNT_e(bblep_unit_rom_IDNT);
  identificateur boe_unit_rom_IDNT("_boe");
  gen boe_unit_rom_IDNT_e(boe_unit_rom_IDNT);
  identificateur bu_unit_rom_IDNT("_bu");
  gen bu_unit_rom_IDNT_e(bu_unit_rom_IDNT);
  identificateur buUS_unit_rom_IDNT("_buUS");
  gen buUS_unit_rom_IDNT_e(buUS_unit_rom_IDNT);
  identificateur c3__unit_rom_IDNT("_c3_");
  gen c3__unit_rom_IDNT_e(c3__unit_rom_IDNT);
  identificateur c__unit_rom_IDNT("_c_");
  gen c__unit_rom_IDNT_e(c__unit_rom_IDNT);
  identificateur cal_unit_rom_IDNT("_cal");
  gen cal_unit_rom_IDNT_e(cal_unit_rom_IDNT);
  identificateur cd_unit_rom_IDNT("_cd");
  gen cd_unit_rom_IDNT_e(cd_unit_rom_IDNT);
  identificateur cf_unit_rom_IDNT("_cf");
  gen cf_unit_rom_IDNT_e(cf_unit_rom_IDNT);
  identificateur chain_unit_rom_IDNT("_chain");
  gen chain_unit_rom_IDNT_e(chain_unit_rom_IDNT);
  identificateur ct_unit_rom_IDNT("_ct");
  gen ct_unit_rom_IDNT_e(ct_unit_rom_IDNT);
  identificateur cu_unit_rom_IDNT("_cu");
  gen cu_unit_rom_IDNT_e(cu_unit_rom_IDNT);
  identificateur d_unit_rom_IDNT("_d");
  gen d_unit_rom_IDNT_e(d_unit_rom_IDNT);
  identificateur dB_unit_rom_IDNT("_dB");
  gen dB_unit_rom_IDNT_e(dB_unit_rom_IDNT);
  identificateur deg_unit_rom_IDNT("_deg");
  gen deg_unit_rom_IDNT_e(deg_unit_rom_IDNT);
  identificateur dyn_unit_rom_IDNT("_dyn");
  gen dyn_unit_rom_IDNT_e(dyn_unit_rom_IDNT);
  identificateur eV_unit_rom_IDNT("_eV");
  gen eV_unit_rom_IDNT_e(eV_unit_rom_IDNT);
  identificateur epsilon0q__unit_rom_IDNT("_epsilon0q_");
  gen epsilon0q__unit_rom_IDNT_e(epsilon0q__unit_rom_IDNT);
  identificateur epsilonox__unit_rom_IDNT("_epsilonox_");
  gen epsilonox__unit_rom_IDNT_e(epsilonox__unit_rom_IDNT);
  identificateur epsilonsi__unit_rom_IDNT("_epsilonsi_");
  gen epsilonsi__unit_rom_IDNT_e(epsilonsi__unit_rom_IDNT);
  identificateur erg_unit_rom_IDNT("_erg");
  gen erg_unit_rom_IDNT_e(erg_unit_rom_IDNT);
  identificateur f0__unit_rom_IDNT("_f0_");
  gen f0__unit_rom_IDNT_e(f0__unit_rom_IDNT);
  identificateur fath_unit_rom_IDNT("_fath");
  gen fath_unit_rom_IDNT_e(fath_unit_rom_IDNT);
  identificateur fbm_unit_rom_IDNT("_fbm");
  gen fbm_unit_rom_IDNT_e(fbm_unit_rom_IDNT);
  identificateur fc_unit_rom_IDNT("_fc");
  gen fc_unit_rom_IDNT_e(fc_unit_rom_IDNT);
  identificateur fermi_unit_rom_IDNT("_fermi");
  gen fermi_unit_rom_IDNT_e(fermi_unit_rom_IDNT);
  identificateur flam_unit_rom_IDNT("_flam");
  gen flam_unit_rom_IDNT_e(flam_unit_rom_IDNT);
  identificateur fm_unit_rom_IDNT("_fm");
  gen fm_unit_rom_IDNT_e(fm_unit_rom_IDNT);
  identificateur ft_unit_rom_IDNT("_ft");
  gen ft_unit_rom_IDNT_e(ft_unit_rom_IDNT);
  identificateur ftUS_unit_rom_IDNT("_ftUS");
  gen ftUS_unit_rom_IDNT_e(ftUS_unit_rom_IDNT);
  identificateur g_unit_rom_IDNT("_g");
  gen g_unit_rom_IDNT_e(g_unit_rom_IDNT);
  identificateur g__unit_rom_IDNT("_g_");
  gen g__unit_rom_IDNT_e(g__unit_rom_IDNT);
  identificateur galC_unit_rom_IDNT("_galC");
  gen galC_unit_rom_IDNT_e(galC_unit_rom_IDNT);
  identificateur galUK_unit_rom_IDNT("_galUK");
  gen galUK_unit_rom_IDNT_e(galUK_unit_rom_IDNT);
  identificateur galUS_unit_rom_IDNT("_galUS");
  gen galUS_unit_rom_IDNT_e(galUS_unit_rom_IDNT);
  identificateur gf_unit_rom_IDNT("_gf");
  gen gf_unit_rom_IDNT_e(gf_unit_rom_IDNT);
  identificateur gmol_unit_rom_IDNT("_gmol");
  gen gmol_unit_rom_IDNT_e(gmol_unit_rom_IDNT);
  identificateur gon_unit_rom_IDNT("_gon");
  gen gon_unit_rom_IDNT_e(gon_unit_rom_IDNT);
  identificateur grad_unit_rom_IDNT("_grad");
  gen grad_unit_rom_IDNT_e(grad_unit_rom_IDNT);
  identificateur grain_unit_rom_IDNT("_grain");
  gen grain_unit_rom_IDNT_e(grain_unit_rom_IDNT);
  identificateur h_unit_rom_IDNT("_h");
  gen h_unit_rom_IDNT_e(h_unit_rom_IDNT);
  identificateur h__unit_rom_IDNT("_h_");
  gen h__unit_rom_IDNT_e(h__unit_rom_IDNT);
  identificateur ha_unit_rom_IDNT("_ha");
  gen ha_unit_rom_IDNT_e(ha_unit_rom_IDNT);
  identificateur hbar__unit_rom_IDNT("_hbar_");
  gen hbar__unit_rom_IDNT_e(hbar__unit_rom_IDNT);
  identificateur hp_unit_rom_IDNT("_hp");
  gen hp_unit_rom_IDNT_e(hp_unit_rom_IDNT);
  identificateur inH2O_unit_rom_IDNT("_inH2O");
  gen inH2O_unit_rom_IDNT_e(inH2O_unit_rom_IDNT);
  identificateur inHg_unit_rom_IDNT("_inHg");
  gen inHg_unit_rom_IDNT_e(inHg_unit_rom_IDNT);
  identificateur inch_unit_rom_IDNT("_inch");
  gen inch_unit_rom_IDNT_e(inch_unit_rom_IDNT);
  identificateur j_unit_rom_IDNT("_j");
  gen j_unit_rom_IDNT_e(j_unit_rom_IDNT);
  identificateur k__unit_rom_IDNT("_k_");
  gen k__unit_rom_IDNT_e(k__unit_rom_IDNT);
  identificateur kip_unit_rom_IDNT("_kip");
  gen kip_unit_rom_IDNT_e(kip_unit_rom_IDNT);
  identificateur knot_unit_rom_IDNT("_knot");
  gen knot_unit_rom_IDNT_e(knot_unit_rom_IDNT);
  identificateur kph_unit_rom_IDNT("_kph");
  gen kph_unit_rom_IDNT_e(kph_unit_rom_IDNT);
  identificateur kq__unit_rom_IDNT("_kq_");
  gen kq__unit_rom_IDNT_e(kq__unit_rom_IDNT);
  identificateur l_unit_rom_IDNT("_l");
  gen l_unit_rom_IDNT_e(l_unit_rom_IDNT);
  identificateur lam_unit_rom_IDNT("_lam");
  gen lam_unit_rom_IDNT_e(lam_unit_rom_IDNT);
  identificateur lambda0_unit_rom_IDNT("_lambda0");
  gen lambda0_unit_rom_IDNT_e(lambda0_unit_rom_IDNT);
  identificateur lambdac__unit_rom_IDNT("_lambdac_");
  gen lambdac__unit_rom_IDNT_e(lambdac__unit_rom_IDNT);
  identificateur lb_unit_rom_IDNT("_lb");
  gen lb_unit_rom_IDNT_e(lb_unit_rom_IDNT);
  identificateur lbf_unit_rom_IDNT("_lbf");
  gen lbf_unit_rom_IDNT_e(lbf_unit_rom_IDNT);
  identificateur lbmol_unit_rom_IDNT("_lbmol");
  gen lbmol_unit_rom_IDNT_e(lbmol_unit_rom_IDNT);
  identificateur lbt_unit_rom_IDNT("_lbt");
  gen lbt_unit_rom_IDNT_e(lbt_unit_rom_IDNT);
  identificateur lep_unit_rom_IDNT("_lep");
  gen lep_unit_rom_IDNT_e(lep_unit_rom_IDNT);
  identificateur liqpt_unit_rom_IDNT("_liqpt");
  gen liqpt_unit_rom_IDNT_e(liqpt_unit_rom_IDNT);
  identificateur lyr_unit_rom_IDNT("_lyr");
  gen lyr_unit_rom_IDNT_e(lyr_unit_rom_IDNT);
  identificateur mEarth__unit_rom_IDNT("_mEarth_");
  gen mEarth__unit_rom_IDNT_e(mEarth__unit_rom_IDNT);
  identificateur mSun__unit_rom_IDNT("_mSun_");
  gen mSun__unit_rom_IDNT_e(mSun__unit_rom_IDNT);
  identificateur me__unit_rom_IDNT("_me_");
  gen me__unit_rom_IDNT_e(me__unit_rom_IDNT);
  identificateur mho_unit_rom_IDNT("_mho");
  gen mho_unit_rom_IDNT_e(mho_unit_rom_IDNT);
  identificateur mi_unit_rom_IDNT("_mi");
  gen mi_unit_rom_IDNT_e(mi_unit_rom_IDNT);
  identificateur miUS_unit_rom_IDNT("_miUS");
  gen miUS_unit_rom_IDNT_e(miUS_unit_rom_IDNT);
  identificateur mil_unit_rom_IDNT("_mil");
  gen mil_unit_rom_IDNT_e(mil_unit_rom_IDNT);
  identificateur mile_unit_rom_IDNT("_mile");
  gen mile_unit_rom_IDNT_e(mile_unit_rom_IDNT);
  identificateur mille_unit_rom_IDNT("_mille");
  gen mille_unit_rom_IDNT_e(mille_unit_rom_IDNT);
  identificateur min_unit_rom_IDNT("_min");
  gen min_unit_rom_IDNT_e(min_unit_rom_IDNT);
  identificateur mmHg_unit_rom_IDNT("_mmHg");
  gen mmHg_unit_rom_IDNT_e(mmHg_unit_rom_IDNT);
  identificateur mn_unit_rom_IDNT("_mn");
  gen mn_unit_rom_IDNT_e(mn_unit_rom_IDNT);
  identificateur mol_unit_rom_IDNT("_mol");
  gen mol_unit_rom_IDNT_e(mol_unit_rom_IDNT);
  identificateur molK_unit_rom_IDNT("_molK");
  gen molK_unit_rom_IDNT_e(molK_unit_rom_IDNT);
  identificateur mp__unit_rom_IDNT("_mp_");
  gen mp__unit_rom_IDNT_e(mp__unit_rom_IDNT);
  identificateur mph_unit_rom_IDNT("_mph");
  gen mph_unit_rom_IDNT_e(mph_unit_rom_IDNT);
  identificateur mpme__unit_rom_IDNT("_mpme_");
  gen mpme__unit_rom_IDNT_e(mpme__unit_rom_IDNT);
  identificateur mu0__unit_rom_IDNT("_mu0_");
  gen mu0__unit_rom_IDNT_e(mu0__unit_rom_IDNT);
  identificateur muB__unit_rom_IDNT("_muB_");
  gen muB__unit_rom_IDNT_e(muB__unit_rom_IDNT);
  identificateur muN__unit_rom_IDNT("_muN_");
  gen muN__unit_rom_IDNT_e(muN__unit_rom_IDNT);
  identificateur nmi_unit_rom_IDNT("_nmi");
  gen nmi_unit_rom_IDNT_e(nmi_unit_rom_IDNT);
  identificateur oz_unit_rom_IDNT("_oz");
  gen oz_unit_rom_IDNT_e(oz_unit_rom_IDNT);
  identificateur ozUK_unit_rom_IDNT("_ozUK");
  gen ozUK_unit_rom_IDNT_e(ozUK_unit_rom_IDNT);
  identificateur ozfl_unit_rom_IDNT("_ozfl");
  gen ozfl_unit_rom_IDNT_e(ozfl_unit_rom_IDNT);
  identificateur ozt_unit_rom_IDNT("_ozt");
  gen ozt_unit_rom_IDNT_e(ozt_unit_rom_IDNT);
  identificateur pc_unit_rom_IDNT("_pc");
  gen pc_unit_rom_IDNT_e(pc_unit_rom_IDNT);
  identificateur pdl_unit_rom_IDNT("_pdl");
  gen pdl_unit_rom_IDNT_e(pdl_unit_rom_IDNT);
  identificateur phi__unit_rom_IDNT("_phi_");
  gen phi__unit_rom_IDNT_e(phi__unit_rom_IDNT);
  identificateur pk_unit_rom_IDNT("_pk");
  gen pk_unit_rom_IDNT_e(pk_unit_rom_IDNT);
  identificateur psi_unit_rom_IDNT("_psi");
  gen psi_unit_rom_IDNT_e(psi_unit_rom_IDNT);
  identificateur pt_unit_rom_IDNT("_pt");
  gen pt_unit_rom_IDNT_e(pt_unit_rom_IDNT);
  identificateur ptUK_unit_rom_IDNT("_ptUK");
  gen ptUK_unit_rom_IDNT_e(ptUK_unit_rom_IDNT);
  identificateur qe_unit_rom_IDNT("_qe");
  gen qe_unit_rom_IDNT_e(qe_unit_rom_IDNT);
  identificateur qepsilon0__unit_rom_IDNT("_qepsilon0_");
  gen qepsilon0__unit_rom_IDNT_e(qepsilon0__unit_rom_IDNT);
  identificateur qme__unit_rom_IDNT("_qme_");
  gen qme__unit_rom_IDNT_e(qme__unit_rom_IDNT);
  identificateur qt_unit_rom_IDNT("_qt");
  gen qt_unit_rom_IDNT_e(qt_unit_rom_IDNT);
  identificateur rad_unit_rom_IDNT("_rad");
  gen rad_unit_rom_IDNT_e(rad_unit_rom_IDNT);
  identificateur rd_unit_rom_IDNT("_rd");
  gen rd_unit_rom_IDNT_e(rd_unit_rom_IDNT);
  identificateur rem_unit_rom_IDNT("_rem");
  gen rem_unit_rom_IDNT_e(rem_unit_rom_IDNT);
  identificateur rev_unit_rom_IDNT("_rev");
  gen rev_unit_rom_IDNT_e(rev_unit_rom_IDNT);
  identificateur rod_unit_rom_IDNT("_rod");
  gen rod_unit_rom_IDNT_e(rod_unit_rom_IDNT);
  identificateur rpm_unit_rom_IDNT("_rpm");
  gen rpm_unit_rom_IDNT_e(rpm_unit_rom_IDNT);
  identificateur s_unit_rom_IDNT("_s");
  gen s_unit_rom_IDNT_e(s_unit_rom_IDNT);
  identificateur sb_unit_rom_IDNT("_sb");
  gen sb_unit_rom_IDNT_e(sb_unit_rom_IDNT);
  identificateur sd_unit_rom_IDNT("_sd");
  gen sd_unit_rom_IDNT_e(sd_unit_rom_IDNT);
  identificateur sigma_unit_rom_IDNT("_sigma");
  gen sigma_unit_rom_IDNT_e(sigma_unit_rom_IDNT);
  identificateur slug_unit_rom_IDNT("_slug");
  gen slug_unit_rom_IDNT_e(slug_unit_rom_IDNT);
  identificateur st_unit_rom_IDNT("_st");
  gen st_unit_rom_IDNT_e(st_unit_rom_IDNT);
  identificateur syr__unit_rom_IDNT("_syr_");
  gen syr__unit_rom_IDNT_e(syr__unit_rom_IDNT);
  identificateur t_unit_rom_IDNT("_t");
  gen t_unit_rom_IDNT_e(t_unit_rom_IDNT);
  identificateur tbsp_unit_rom_IDNT("_tbsp");
  gen tbsp_unit_rom_IDNT_e(tbsp_unit_rom_IDNT);
  identificateur tec_unit_rom_IDNT("_tec");
  gen tec_unit_rom_IDNT_e(tec_unit_rom_IDNT);
  identificateur tep_unit_rom_IDNT("_tep");
  gen tep_unit_rom_IDNT_e(tep_unit_rom_IDNT);
  identificateur tepC_unit_rom_IDNT("_tepC");
  gen tepC_unit_rom_IDNT_e(tepC_unit_rom_IDNT);
  identificateur tepcC_unit_rom_IDNT("_tepcC");
  gen tepcC_unit_rom_IDNT_e(tepcC_unit_rom_IDNT);
  identificateur tepgC_unit_rom_IDNT("_tepgC");
  gen tepgC_unit_rom_IDNT_e(tepgC_unit_rom_IDNT);
  identificateur tex_unit_rom_IDNT("_tex");
  gen tex_unit_rom_IDNT_e(tex_unit_rom_IDNT);
  identificateur therm_unit_rom_IDNT("_therm");
  gen therm_unit_rom_IDNT_e(therm_unit_rom_IDNT);
  identificateur toe_unit_rom_IDNT("_toe");
  gen toe_unit_rom_IDNT_e(toe_unit_rom_IDNT);
  identificateur ton_unit_rom_IDNT("_ton");
  gen ton_unit_rom_IDNT_e(ton_unit_rom_IDNT);
  identificateur tonUK_unit_rom_IDNT("_tonUK");
  gen tonUK_unit_rom_IDNT_e(tonUK_unit_rom_IDNT);
  identificateur tr_unit_rom_IDNT("_tr");
  gen tr_unit_rom_IDNT_e(tr_unit_rom_IDNT);
  identificateur tsp_unit_rom_IDNT("_tsp");
  gen tsp_unit_rom_IDNT_e(tsp_unit_rom_IDNT);
  identificateur u_unit_rom_IDNT("_u");
  gen u_unit_rom_IDNT_e(u_unit_rom_IDNT);
  identificateur E_unit_rom_IDNT("_E");
  gen E_unit_rom_IDNT_e(E_unit_rom_IDNT);
  identificateur Curie_unit_rom_IDNT("_Curie");
  gen Curie_unit_rom_IDNT_e(Curie_unit_rom_IDNT);
  identificateur C_unit_rom_IDNT("_C");
  gen C_unit_rom_IDNT_e(C_unit_rom_IDNT);
  identificateur kg_unit_rom_IDNT("_kg");
  gen kg_unit_rom_IDNT_e(kg_unit_rom_IDNT);
  // gen _kg_unit(kg_unit_rom_IDNT);
  identificateur Angstrom_unit_rom_IDNT("_Angstrom");
  gen Angstrom_unit_rom_IDNT_e(Angstrom_unit_rom_IDNT);
#endif
  const gen * const tab_unit_rom[]={
    &A_unit_rom_IDNT_e,
    &Angstrom_unit_rom_IDNT_e,
    &Bq_unit_rom_IDNT_e,
    &Btu_unit_rom_IDNT_e,
    &C_unit_rom_IDNT_e,
    &Curie_unit_rom_IDNT_e,
    &E_unit_rom_IDNT_e,
    &F_unit_rom_IDNT_e,
    &FF_unit_rom_IDNT_e,
    &Faraday__unit_rom_IDNT_e,
    &Fdy_unit_rom_IDNT_e,
    &G__unit_rom_IDNT_e,
    &Gal_unit_rom_IDNT_e,
    &Gy_unit_rom_IDNT_e,
    &H_unit_rom_IDNT_e,
    &HFCC_unit_rom_IDNT_e,
    &Hz_unit_rom_IDNT_e,
    &IO__unit_rom_IDNT_e,
    &J_unit_rom_IDNT_e,
    &K_unit_rom_IDNT_e,
    &L_unit_rom_IDNT_e,
    &N_unit_rom_IDNT_e,
    &NA__unit_rom_IDNT_e,
    &Ohm_unit_rom_IDNT_e,
    &P_unit_rom_IDNT_e,
    &PSun__unit_rom_IDNT_e,
    &Pa_unit_rom_IDNT_e,
    &R_unit_rom_IDNT_e,
    &REarth__unit_rom_IDNT_e,
    &RSun__unit_rom_IDNT_e,
    &R__unit_rom_IDNT_e,
    &Rankine_unit_rom_IDNT_e,
    &Rinfinity__unit_rom_IDNT_e,
    &S_unit_rom_IDNT_e,
    &St_unit_rom_IDNT_e,
    &StdP__unit_rom_IDNT_e,
    &StdT__unit_rom_IDNT_e,
    &Sv_unit_rom_IDNT_e,
    &T_unit_rom_IDNT_e,
    &Torr_unit_rom_IDNT_e,
    &V_unit_rom_IDNT_e,
    &Vm__unit_rom_IDNT_e,
    &W_unit_rom_IDNT_e,
    &Wb_unit_rom_IDNT_e,
    &Wh_unit_rom_IDNT_e,
    &a_unit_rom_IDNT_e,
    &a0__unit_rom_IDNT_e,
    &acre_unit_rom_IDNT_e,
    &alpha__unit_rom_IDNT_e,
    &arcmin_unit_rom_IDNT_e,
    &arcs_unit_rom_IDNT_e,
    &atm_unit_rom_IDNT_e,
    &au_unit_rom_IDNT_e,
    &b_unit_rom_IDNT_e,
    &bar_unit_rom_IDNT_e,
    &bbl_unit_rom_IDNT_e,
    &bblep_unit_rom_IDNT_e,
    &boe_unit_rom_IDNT_e,
    &bu_unit_rom_IDNT_e,
    &buUS_unit_rom_IDNT_e,
    &c3__unit_rom_IDNT_e,
    &c__unit_rom_IDNT_e,
    &cal_unit_rom_IDNT_e,
    &cd_unit_rom_IDNT_e,
    &cf_unit_rom_IDNT_e,
    &chain_unit_rom_IDNT_e,
    &ct_unit_rom_IDNT_e,
    &cu_unit_rom_IDNT_e,
    &d_unit_rom_IDNT_e,
    &dB_unit_rom_IDNT_e,
    &deg_unit_rom_IDNT_e,
    &dyn_unit_rom_IDNT_e,
    &eV_unit_rom_IDNT_e,
    &epsilon0__unit_rom_IDNT_e,
    &epsilon0q__unit_rom_IDNT_e,
    &epsilonox__unit_rom_IDNT_e,
    &epsilonsi__unit_rom_IDNT_e,
    &erg_unit_rom_IDNT_e,
    &f0__unit_rom_IDNT_e,
    &fath_unit_rom_IDNT_e,
    &fbm_unit_rom_IDNT_e,
    &fc_unit_rom_IDNT_e,
    &fermi_unit_rom_IDNT_e,
    &flam_unit_rom_IDNT_e,
    &fm_unit_rom_IDNT_e,
    &ft_unit_rom_IDNT_e,
    &ftUS_unit_rom_IDNT_e,
    &g_unit_rom_IDNT_e,
    &g__unit_rom_IDNT_e,
    &galC_unit_rom_IDNT_e,
    &galUK_unit_rom_IDNT_e,
    &galUS_unit_rom_IDNT_e,
    &gf_unit_rom_IDNT_e,
    &gmol_unit_rom_IDNT_e,
    &gon_unit_rom_IDNT_e,
    &grad_unit_rom_IDNT_e,
    &grain_unit_rom_IDNT_e,
    &h_unit_rom_IDNT_e,
    &h__unit_rom_IDNT_e,
    &ha_unit_rom_IDNT_e,
    &hbar__unit_rom_IDNT_e,
    &hp_unit_rom_IDNT_e,
    &inH2O_unit_rom_IDNT_e,
    &inHg_unit_rom_IDNT_e,
    &inch_unit_rom_IDNT_e,
    &j_unit_rom_IDNT_e,
    &k__unit_rom_IDNT_e,
    &kg_unit_rom_IDNT_e,
    &kip_unit_rom_IDNT_e,
    &knot_unit_rom_IDNT_e,
    &kph_unit_rom_IDNT_e,
    &kq__unit_rom_IDNT_e,
    &l_unit_rom_IDNT_e,
    &lam_unit_rom_IDNT_e,
    &lambda0_unit_rom_IDNT_e,
    &lambdac__unit_rom_IDNT_e,
    &lb_unit_rom_IDNT_e,
    &lbf_unit_rom_IDNT_e,
    &lbmol_unit_rom_IDNT_e,
    &lbt_unit_rom_IDNT_e,
    &lep_unit_rom_IDNT_e,
    &liqpt_unit_rom_IDNT_e,
    &lyr_unit_rom_IDNT_e,
    &m_unit_rom_IDNT_e,
    &mEarth__unit_rom_IDNT_e,
    &mSun__unit_rom_IDNT_e,
    &me__unit_rom_IDNT_e,
    &mho_unit_rom_IDNT_e,
    &mi_unit_rom_IDNT_e,
    &miUS_unit_rom_IDNT_e,
    &mil_unit_rom_IDNT_e,
    &mile_unit_rom_IDNT_e,
    &mille_unit_rom_IDNT_e,
    &min_unit_rom_IDNT_e,
    &mmHg_unit_rom_IDNT_e,
    &mn_unit_rom_IDNT_e,
    &mol_unit_rom_IDNT_e,
    &molK_unit_rom_IDNT_e,
    &mp__unit_rom_IDNT_e,
    &mph_unit_rom_IDNT_e,
    &mpme__unit_rom_IDNT_e,
    &mu0__unit_rom_IDNT_e,
    &muB__unit_rom_IDNT_e,
    &muN__unit_rom_IDNT_e,
    &nmi_unit_rom_IDNT_e,
    &oz_unit_rom_IDNT_e,
    &ozUK_unit_rom_IDNT_e,
    &ozfl_unit_rom_IDNT_e,
    &ozt_unit_rom_IDNT_e,
    &pc_unit_rom_IDNT_e,
    &pdl_unit_rom_IDNT_e,
    &phi__unit_rom_IDNT_e,
    &pk_unit_rom_IDNT_e,
    &psi_unit_rom_IDNT_e,
    &pt_unit_rom_IDNT_e,
    &ptUK_unit_rom_IDNT_e,
    &qe_unit_rom_IDNT_e,
    &qepsilon0__unit_rom_IDNT_e,
    &qme__unit_rom_IDNT_e,
    &qt_unit_rom_IDNT_e,
    &rad_unit_rom_IDNT_e,
    &rd_unit_rom_IDNT_e,
    &rem_unit_rom_IDNT_e,
    &rev_unit_rom_IDNT_e,
    &rod_unit_rom_IDNT_e,
    &rpm_unit_rom_IDNT_e,
    &s_unit_rom_IDNT_e,
    &sb_unit_rom_IDNT_e,
    &sd_unit_rom_IDNT_e,
    &sigma_unit_rom_IDNT_e,
    &slug_unit_rom_IDNT_e,
    &st_unit_rom_IDNT_e,
    &syr__unit_rom_IDNT_e,
    &t_unit_rom_IDNT_e,
    &tbsp_unit_rom_IDNT_e,
    &tec_unit_rom_IDNT_e,
    &tep_unit_rom_IDNT_e,
    &tepC_unit_rom_IDNT_e,
    &tepcC_unit_rom_IDNT_e,
    &tepgC_unit_rom_IDNT_e,
    &tex_unit_rom_IDNT_e,
    &therm_unit_rom_IDNT_e,
    &toe_unit_rom_IDNT_e,
    &ton_unit_rom_IDNT_e,
    &tonUK_unit_rom_IDNT_e,
    &tr_unit_rom_IDNT_e,
    &tsp_unit_rom_IDNT_e,
    &u_unit_rom_IDNT_e,
    &yd_unit_rom_IDNT_e,
    &yr_unit_rom_IDNT_e,
  };
  bool is_unit_rom(const char * s,gen & res){
    for (int i=0;i<sizeof(tab_unit_rom)/sizeof(gen *);++i){
      if (!strcmp(s,tab_unit_rom[i]->_IDNTptr->id_name)){
	res=*tab_unit_rom[i];
	return true;
      }
    }
    return false;
  }


  gen mksa_register(const char * s,const mksa_unit * equiv){
#ifdef USTL
    ustl::map<const char *, const mksa_unit *,ltstr>::const_iterator it=unit_conversion_map().find(s+1),itend=unit_conversion_map().end();
#else
    std::map<const char *, const mksa_unit *,ltstr>::const_iterator it=unit_conversion_map().find(s+1),itend=unit_conversion_map().end();
#endif
    gen res;
    lock_syms_mutex();  
    if (it!=itend)
      res=syms()[s];
    else {
      unit_conversion_map()[s+1]=equiv;
      if (is_unit_rom(s,res))
	syms()[s]=res;
      else
	res = (syms()[s] = new ref_identificateur(s));
    }
    unlock_syms_mutex();  
    return res;
  }
  gen mksa_register_unit(const char * s,const mksa_unit * equiv){
    return symbolic(at_unit,makevecteur(1,mksa_register(s,equiv)));
  }
  const gen * tab_usual_units[]={&C_unit_rom_IDNT_e,&F_unit_rom_IDNT_e,&Gy_unit_rom_IDNT_e,&H_unit_rom_IDNT_e,&Hz_unit_rom_IDNT_e,&J_unit_rom_IDNT_e,&mho_unit_rom_IDNT_e,&N_unit_rom_IDNT_e,&Ohm_unit_rom_IDNT_e,&Pa_unit_rom_IDNT_e,&rad_unit_rom_IDNT_e,&S_unit_rom_IDNT_e,&Sv_unit_rom_IDNT_e,&T_unit_rom_IDNT_e,&V_unit_rom_IDNT_e,&W_unit_rom_IDNT_e,&Wb_unit_rom_IDNT_e};

  
  const mksa_unit __Angstrom_unit={1e-10,1,0,0,0,0,0,0,0};
  const mksa_unit __Btu_unit={1055.05585262,2,1,-2,0,0,0,0,0};
  const mksa_unit __Curie_unit={3.7e10,0,0,-1,0,0,0,0,0};
  const mksa_unit __FF_unit={.152449017237,0,0,0,0,0,0,0,1};
  const mksa_unit __Fdy_unit={96485.3365,0,0,1,1,0,0,0,0};
  const mksa_unit __Gal={0.01,1,0,-2,0,0,0,0,0};
  const mksa_unit __HFCC_unit={1400,1,0,0,0,0,0,0,0};
  const mksa_unit __L_unit={0.001,3,0,0,0,0,0,0,0};
  const mksa_unit __P_unit={.1,-1,1,-1,0,0,0,0,0};
  const mksa_unit __R_unit={0.000258,0,-1,1,1,0,0,0,0};
  const mksa_unit __Rankine_unit={5./9,0,0,0,0,1,0,0,0};
  const mksa_unit __St_unit={0.0001,2,0,-1,0,0,0,0,0};
  const mksa_unit __Wh_unit={3600,2,1,-2,0,0,0,0,0};
  const mksa_unit __a_unit={100,2,0,0,0,0,0,0,0};
  const mksa_unit __acre_unit={4046.87260987,2,0,0,0,0,0,0,0};
  const mksa_unit __arcmin_unit={2.90888208666e-4,0,0,0,0,0,0,0,0};
  const mksa_unit __arcs_unit={4.8481368111e-6,0,0,0,0,0,0,0,0};
  const mksa_unit __atm_unit={101325.0,-1,1,-2,0,0,0,0,0};
  const mksa_unit __au_unit={1.495979e11,1,0,0,0,0,0,0,0};
  const mksa_unit __b_unit={1e-28,2,0,0,0,0,0,0,0};
  const mksa_unit __bar_unit={1e5,-1,1,-2,0,0,0,0,0};
  const mksa_unit __bbl_unit={.158987294928,3,0,0,0,0,0,0,0};
  const mksa_unit __bblep_unit={.158987294928*0.857*41.76e9,2,1,-2,0,0,0,0,0};
  const mksa_unit __boe_unit={.158987294928*0.857*41.76e9,2,1,-2,0,0,0,0,0};
  const mksa_unit __bu={0.036368736,3,0,0,0,0,0,0,0};
  const mksa_unit __buUS={0.03523907,3,0,0,0,0,0,0,0};
  const mksa_unit __cal_unit={4.184,2,1,-2,0,0,0,0,0};
  const mksa_unit __cf_unit={1.08e6,2,1,-2,0,0,0,0,0};
  const mksa_unit __chain_unit={20.1168402337,1,0,0,0,0,0,0,0};
  const mksa_unit __ct_unit={0.0002,0,1,0,0,0,0,0,0};
  const mksa_unit __dB_unit={1,0,0,0,0,0,0,0,0};
  const mksa_unit __d_unit={86400,0,0,1,0,0,0,0,0};
  const mksa_unit __deg_unit={1.74532925199e-2,0,0,0,0,0,0,0,0};
  // const mksa_unit __degreeF_unit={5./9,0,0,0,0,1,0,0,0};
  const mksa_unit __dyn_unit={1e-5,1,1,-2,0,0,0,0,0};
  const mksa_unit __eV_unit={1.602176634e-19,2,1,-2,0,0,0,0,0}; // Electron volt
  const mksa_unit __erg_unit={1e-7,2,1,-2,0,0,0,0,0};
  const mksa_unit __fath_unit={1.82880365761,1,0,0,0,0,0,0,0};
  const mksa_unit __fbm_unit={0.002359737216,3,0,0,0,0,0,0,0};
  const mksa_unit __fc_unit={10.7639104167,1,0,0,0,0,0,0,0};
  const mksa_unit __fermi_unit={1e-15,1,0,0,0,0,0,0,0};
  const mksa_unit __flam_unit={3.42625909964,-2,0,0,0,0,0,1,0};
  const mksa_unit __fm_unit={1.82880365761,1,0,0,0,0,0,0,0};
  const mksa_unit __ft_unit={0.3048,1,0,0,0,0,0,0,0};
  const mksa_unit __ftUS_unit={0.304800609601,1,0,0,0,0,0,0,0};
  const mksa_unit __g_unit={1e-3,0,1,0,0,0,0,0,0};
  const mksa_unit __galC_unit={0.00454609,3,0,0,0,0,0,0,0};
  const mksa_unit __galUK_unit={0.004546092,3,0,0,0,0,0,0,0};
  const mksa_unit __galUS_unit={0.003785411784,3,0,0,0,0,0,0,0};
  const mksa_unit __cu_unit={0.000236588236373,3,0,0,0,0,0,0,0};
  const mksa_unit __gf_unit={0.00980665,1,1,-2,0,0,0,0,0};
  const mksa_unit __gmol_unit={1,0,0,0,0,0,1,0,0};
  const mksa_unit __gon_unit={1.57079632679e-2,0,0,0,0,0,0,0};
  const mksa_unit __grad_unit={1.57079632679e-2,0,0,0,0,0,0,0,0};
  const mksa_unit __grain_unit={0.00006479891,0,1,0,0,0,0,0,0};
  const mksa_unit __h_unit={3600,0,0,1,0,0,0,0,0};
  const mksa_unit __ha_unit={10000,2,0,0,0,0,0,0,0};
  const mksa_unit __hp_unit={745.699871582,2,1,-3,0,0,0,0,0};
  const mksa_unit __in_unit={0.0254,1,0,0,0,0,0,0,0};
  const mksa_unit __inH2O_unit={249.08193551052,-1,1,-2,0,0,0,0,0};
  const mksa_unit __inHg_unit={3386.38815789,-1,1,-2,0,0,0,0,0};
  const mksa_unit __j_unit={86400,0,0,1,0,0,0,0,0};
  const mksa_unit __kip_unit={4448.22161526,1,1,-2,0,0,0,0,0};
  const mksa_unit __knot_unit={0.51444444444,1,0,-1,0,0,0,0,0};
  const mksa_unit __kph_unit={0.2777777777777,1,0,-1,0,0,0,0,0};
  const mksa_unit __l_unit={0.001,3,0,0,0,0,0,0,0};
  const mksa_unit __lam_unit={3183.09886184,-2,0,0,0,0,0,1,0};
  const mksa_unit __lb_unit={0.45359237,0,1,0,0,0,0,0,0};
  const mksa_unit __lbf_unit={4.44822161526,1,1,-2,0,0,0,0,0};
  //const mksa_unit __lbf_unit={4.44922161526,1,1,-2,0,0,0,0,0};
  const mksa_unit __lbmol_unit={453.59237,0,0,0,0,0,1,0,0};
  const mksa_unit __lbt_unit={0.3732417216,0,1,0,0,0,0,0,0};
  const mksa_unit __lep_unit={0.857*41.76e6,2,1,-2,0,0,0,0,0};
  const mksa_unit __liqpt_unit={0.000473176473,3,0,0,0,0,0,0,0};
  const mksa_unit __lyr_unit={9.46052840488e15,1,0,0,0,0,0,0,0};
  const mksa_unit __mi_unit={1609.344,1,0,0,0,0,0,0,0};
  const mksa_unit __miUS_unit={1609.34721869,1,0,0,0,0,0,0,0};
  const mksa_unit __mil_unit={0.0000254,1,0,0,0,0,0,0,0};
  const mksa_unit __mile_unit={1609.344,1,0,0,0,0,0,0,0};
  const mksa_unit __mille_unit={1852,1,0,0,0,0,0,0,0};
  const mksa_unit __mn_unit={60,0,0,1,0,0,0,0,0};
  const mksa_unit __mmHg_unit={133.322387415,-1,1,-2,0,0,0,0,0};
  const mksa_unit __molK_unit={1,0,0,0,0,1,1,0,0};
  const mksa_unit __mph_unit={0.44704,1,0,-1,0,0,0,0,0};
  const mksa_unit __nmi_unit={1852,1,0,0,0,0,0,0,0};
  const mksa_unit __oz_unit={0.028349523125,0,1,0,0,0,0,0,0};
  const mksa_unit __ozUK_unit={2.84130625e-5,3,0,0,0,0,0,0,0};
  const mksa_unit __ozfl_unit={2.95735295625e-5,3,0,0,0,0,0,0,0};
  const mksa_unit __ozt_unit={0.0311034768,0,1,0,0,0,0,0,0};
  const mksa_unit __pc_unit={3.08567758149137e16,1,0,0,0,0,0,0,0};
  const mksa_unit __pdl_unit={0.138254954376,1,1,-2,0,0,0,0,0};
  const mksa_unit __pk_unit={0.0088097675,3,0,0,0,0,0,0,0};
  const mksa_unit __psi_unit={6894.75729317,-1,1,-2,0,0,0,0,0};
  const mksa_unit __pt_unit={0.000473176473,3,0,0,0,0,0,0,0};
  const mksa_unit __ptUK_unit={0.0005682615,3,0,0,0,0,0,0,0};
  const mksa_unit __qt_unit={0.000946359246,3,0,0,0,0,0,0,0};
  const mksa_unit __rd_unit={0.01,2,0,-2,0,0,0,0,0};
  const mksa_unit __rem_unit={0.01,2,0,-2,0,0,0,0,0};
  const mksa_unit __rod_unit={5.02921005842,1,0,0,0,0,0,0,0};
  const mksa_unit __rpm_unit={0.0166666666667,0,0,-1,0,0,0,0,0};
  const mksa_unit __sb_unit={10000,-2,0,0,0,0,0,1,0};
  const mksa_unit __slug_unit={14.5939029372,0,1,0,0,0,0,0,0};
  const mksa_unit __st_unit={1,3,0,0,0,0,0,0,0};
  const mksa_unit __t_unit={1000,0,1,0,0,0,0,0,0};
  const mksa_unit __tbsp_unit={1.47867647813e-5,3,0,0,0,0,0,0,0};
  const mksa_unit __tec_unit={41.76e9/1.5,2,1,-2,0,0,0,0,0};
  const mksa_unit __tep_unit={41.76e9,2,1,-2,0,0,0,0,0};
  const mksa_unit __tepC_unit={830,1,0,0,0,0,0,0,0};
  const mksa_unit __tepcC_unit={1000,1,0,0,0,0,0,0,0};
  const mksa_unit __tepgC_unit={650,1,0,0,0,0,0,0,0};
  const mksa_unit __tex={1e-6,-1,1,0,0,0,0,0,0};
  const mksa_unit __therm_unit={105506000,2,1,-2,0,0,0,0,0};
  const mksa_unit __toe_unit={41.76e9,2,1,-2,0,0,0,0,0};
  const mksa_unit __ton_unit={907.18474,0,1,0,0,0,0,0,0};
  const mksa_unit __tonUK_unit={1016.0469088,0,1,0,0,0,0,0,0};
  const mksa_unit __Torr_unit={133.322368421,-1,1,-2,0,0,0,0,0};
  const mksa_unit __tr_unit={2*M_PI,0,0,0,0,0,0,0,0};
  const mksa_unit __tsp_unit={4.928921614571597e-6,3,0,0,0,0,0,0,0};
  const mksa_unit __u_unit={1.6605402e-27,0,1,0,0,0,0,0,0};
  const mksa_unit __yd_unit={0.9144,1,0,0,0,0,0,0,0};
  const mksa_unit __yr_unit={31556925.9747,0,0,1,0,0,0,0,0};
  const mksa_unit __micron_unit={1e-6,1,0,0,0,0,0,0,0};

  const mksa_unit __hbar_unit={1.054571817e-34,2,1,-1,0,0,0,0}; // Reduced Planck constant
  const mksa_unit __c_unit={299792458,1,0,-1,0,0,0,0};        
  const mksa_unit __g__unit={9.80665,1,0,-2,0,0,0,0};       
  const mksa_unit __IO_unit={1e-12,0,1,-3,0,0,0,0}; 
  const mksa_unit __epsilonox_unit={3.9,0,0,0,0,0,0,0}; 
  const mksa_unit __epsilonsi_unit={11.9,0,0,0,0,0,0,0,0}; 
  const mksa_unit __qepsilon0_unit={1.4185979e-30,-3,-1,5,3,0,0,0}; 
  const mksa_unit __epsilon0q_unit={55263469.6,-3,-1,3,1,0,0,0}; 
  const mksa_unit __kq_unit={8.617386e-5,2,1,-3,-1,-1,0,0}; 
  const mksa_unit __c3_unit={.002897756,1,0,0,0,1,0,0}; 
  const mksa_unit __lambdac_unit={2.42631023867e-12,1,0,0,0,0,0,0,0}; // Compton wavelength
  const mksa_unit __f0_unit={2.4179883e14,0,0,-1,0,0,0,0}; 
const mksa_unit __lambda0_unit={1.239841984e-6,1,0,0,0,0,0,0}; // inverse meter-electron volt relationship
  const mksa_unit __muN_unit={5.0507837461e-27,2,0,0,1,0,0,0}; // Nuclear magneton
  const mksa_unit __muB_unit={9.2740100783e-24,2,0,0,1,0,0,0}; // Bohr magneton
  const mksa_unit __a0_unit={5.29177210903e-11,1,0,0,0,0,0,0}; // Bohr radius 
  const mksa_unit __Rinfinity_unit={10973731.568160,-1,0,0,0,0,0,0}; // Rydberg constant  
  const mksa_unit __Faraday_unit={96485.33212,0,0,1,1,0,-1,0}; // Faraday constant
  const mksa_unit __phi_unit={2.067833848e-15,2,1,-2,-1,0,0,0}; // magnetic flux quantum
  const mksa_unit __alpha_unit={7.2973525693e-3,0,0,0,0,0,0,0}; // fine-structure constant
  const mksa_unit __mpme_unit={1836.15267343,0,0,0,0,0,0,0}; // proton-electron mass ratio
  const mksa_unit __mp_unit={1.67262192369e-27,0,1,0,0,0,0,0}; // Proton mass
  const mksa_unit __qme_unit={1.75882001076e11,0,-1,1,1,0,0,0}; // electron charge to mass quotient
  const mksa_unit __me_unit={9.1093837015e-31,0,1,0,0,0,0,0}; // Electron mass
  const mksa_unit __qe_unit={1.602176634e-19,0,0,1,1,0,0,0}; // elementary charge
  const mksa_unit __h__unit={6.62607015e-34,2,1,-1,0,0,0,0}; // Planck constant
  const mksa_unit __G_unit={6.67430e-11,3,-1,-2,0,0,0,0}; // Newtonian constant of gravitation
  const mksa_unit __mu0_unit={1.25663706212e-6,1,1,-2,-2,0,0,0}; // Vacuum magnetic permeability
  const mksa_unit __epsilon0_unit={8.8541878128e-12,-3,-1,4,2,0,0,0}; // Vacuum electric permittivity
  const mksa_unit __sigma_unit={5.670374419e-8,0,1,-3,0,-4,0,0}; // Stefan-Boltzmann constant
  const mksa_unit __StdP_unit={101325.0,-1,1,-2,0,0,0,0}; 
  const mksa_unit __StdT_unit={273.15,0,0,0,0,1,0,0}; 
  const mksa_unit __R__unit={8.314462618,2,1,-2,0,-1,-1,0}; // molar gas constant
  const mksa_unit __Vm_unit={22.41396954e-3,3,0,0,0,0,-1,0}; // molar volume of ideal gas (273.15 K, 101.325 kPa)
  const mksa_unit __k_unit={1.380649e-23,2,1,-2,0,-1,0,0}; // Boltzmann constant
  const mksa_unit __NA_unit={6.02214076e23,0,0,0,0,0,-1,0}; // Avogadro constant
  const mksa_unit __mSun_unit={1.989e30,0,1,0,0,0,0,0}; 
  const mksa_unit __RSun_unit={6.955e8,1,0,0,0,0,0,0}; 
  const mksa_unit __PSun_unit={3.846e26,2,1,-3,0,0,0,0}; 
  const mksa_unit __mEarth_unit={5.9722e24,0,1,0,0,0,0,0}; // Earth mass
  const mksa_unit __REarth_unit={6.371e6,1,0,0,0,0,0,0}; 
  const mksa_unit __sd_unit={8.61640905e4,0,0,1,0,0,0,0}; 
  const mksa_unit __syr_unit={3.15581498e7,0,0,1,0,0,0,0}; 

  // table of alpha-sorted units
  const mksa_unit * const unitptr_tab[]={
    &__A_unit,
    &__Angstrom_unit,
    &__Bq_unit,
    &__Btu_unit,
    &__C_unit,
    &__Curie_unit,
    &__E_unit,
    &__F_unit,
    &__FF_unit,
    &__Faraday_unit,
    &__Fdy_unit,
    &__G_unit,
    &__Gal,
    &__Gy_unit,
    &__H_unit,
    &__HFCC_unit,
    &__Hz_unit,
    &__IO_unit,
    &__J_unit,
    &__K_unit,
    &__L_unit,
    &__N_unit,
    &__NA_unit,
    &__Ohm_unit,
    &__P_unit,
    &__PSun_unit,
    &__Pa_unit,
    &__R_unit,
    &__REarth_unit,
    &__RSun_unit,
    &__R__unit,
    &__Rankine_unit,
    &__Rinfinity_unit,
    &__S_unit,
    &__St_unit,
    &__StdP_unit,
    &__StdT_unit,
    &__Sv_unit,
    &__T_unit,
    &__Torr_unit,
    &__V_unit,
    &__Vm_unit,
    &__W_unit,
    &__Wb_unit,
    &__Wh_unit,
    &__a_unit,
    &__a0_unit,
    &__acre_unit,
    &__alpha_unit,
    &__arcmin_unit,
    &__arcs_unit,
    &__atm_unit,
    &__au_unit,
    &__b_unit,
    &__bar_unit,
    &__bbl_unit,
    &__bblep_unit,
    &__boe_unit,
    &__bu,
    &__buUS,
    &__c3_unit,
    &__c_unit,
    &__cal_unit,
    &__cd_unit,
    &__cf_unit,
    &__chain_unit,
    &__ct_unit,
    &__cu_unit,
    &__d_unit,
    &__dB_unit,
    &__deg_unit,
    // &__degreeF_unit,
    &__dyn_unit,
    &__eV_unit,
    &__epsilon0_unit,
    &__epsilon0q_unit,
    &__epsilonox_unit,
    &__epsilonsi_unit,
    &__erg_unit,
    &__f0_unit,
    &__fath_unit,
    &__fbm_unit,
    &__fc_unit,
    &__fermi_unit,
    &__flam_unit,
    &__fm_unit,
    &__ft_unit,
    &__ftUS_unit,
    &__g_unit,
    &__g__unit,
    &__galC_unit,
    &__galUK_unit,
    &__galUS_unit,
    &__gf_unit,
    &__gmol_unit,
    &__gon_unit,
    &__grad_unit,
    &__grain_unit,
    &__h_unit,
    &__h__unit,
    &__ha_unit,
    &__hbar_unit,
    &__hp_unit,
    &__inH2O_unit,
    &__inHg_unit,
    &__in_unit,
    &__j_unit,
    &__k_unit,
    &__kg_unit,
    &__kip_unit,
    &__knot_unit,
    &__kph_unit,
    &__kq_unit,
    &__l_unit,
    &__lam_unit,
    &__lambda0_unit,
    &__lambdac_unit,
    &__lb_unit,
    &__lbf_unit,
    &__lbmol_unit,
    &__lbt_unit,
    &__lep_unit,
    &__liqpt_unit,
    &__lyr_unit,
    &__m_unit,
    &__mEarth_unit,
    &__mSun_unit,
    &__me_unit,
    &__mho_unit,
    &__mi_unit,
    &__miUS_unit,
    &__mil_unit,
    &__mile_unit,
    &__mille_unit,
    &__mn_unit,
    &__mmHg_unit,
    &__mn_unit,
    &__mol_unit,
    &__molK_unit,
    &__mp_unit,
    &__mph_unit,
    &__mpme_unit,
    &__mu0_unit,
    &__muB_unit,
    &__muN_unit,
    &__nmi_unit,
    &__oz_unit,
    &__ozUK_unit,
    &__ozfl_unit,
    &__ozt_unit,
    &__pc_unit,
    &__pdl_unit,
    &__phi_unit,
    &__pk_unit,
    &__psi_unit,
    &__pt_unit,
    &__ptUK_unit,
    &__qe_unit,
    &__qepsilon0_unit,
    &__qme_unit,
    &__qt_unit,
    &__rad_unit,
    &__rd_unit,
    &__rem_unit,
    &__tr_unit,
    &__rod_unit,
    &__rpm_unit,
    &__s_unit,
    &__sb_unit,
    &__sd_unit,
    &__sigma_unit,
    &__slug_unit,
    &__st_unit,
    &__syr_unit,
    &__t_unit,
    &__tbsp_unit,
    &__tec_unit,
    &__tep_unit,
    &__tepC_unit,
    &__tepcC_unit,
    &__tepgC_unit,
    &__tex,
    &__therm_unit,
    &__toe_unit,
    &__tonUK_unit,
    &__ton_unit,
    &__tr_unit,
    &__tsp_unit,
    &__u_unit,
    &__yd_unit,
    &__yr_unit,
#ifndef VISUALC
    &__micron_unit
#endif
  };
  const unsigned unitptr_tab_length=sizeof(unitptr_tab)/sizeof(mksa_unit *);
  const char * const unitname_tab[]={
    "_A",
    "_Angstrom",
    "_Bq",
    "_Btu",
    "_C",
    "_Curie",
    "_E",
    "_F",
    "_FF",
    "_F_",
    "_Fdy",
    "_G_",
    "_Gal",
    "_Gy",
    "_H",
    "_HFCC",
    "_Hz",
    "_IO_",
    "_J",
    "_K",
    "_L",
    "_N",
    "_NA_",
    "_Ohm",
    "_P",
    "_PSun_",
    "_Pa",
    "_R",
    "_REarth_",
    "_RSun_",
    "_R_",
    "_Rankine",
    "_Rinfinity_",
    "_S",
    "_St",
    "_StdP_",
    "_StdT_",
    "_Sv",
    "_T",
    "_Torr",
    "_V",
    "_Vm_",
    "_W",
    "_Wb",
    "_Wh",
    "_a",
    "_a0_",
    "_acre",
    "_alpha_",
    "_arcmin",
    "_arcs",
    "_atm",
    "_au",
    "_b",
    "_bar",
    "_bbl",
    "_bblep",
    "_boe",
    "_bu",
    "_buUS",
    "_c3_",
    "_c_",
    "_cal",
    "_cd",
    "_cf",
    "_chain",
    "_ct",
    "_cu",
    "_d",
    "_dB",
    "_deg",
    // "_degreeF",
    "_dyn",
    "_eV",
    "_epsilon0_",
    "_epsilon0q_",
    "_epsilonox_",
    "_epsilonsi_",
    "_erg",
    "_f0_",
    "_fath",
    "_fbm",
    "_fc",
    "_fermi",
    "_flam",
    "_fm",
    "_ft",
    "_ftUS",
    "_g",
    "_g_",
    "_galC",
    "_galUK",
    "_galUS",
    "_gf",
    "_gmol",
    "_gon",
    "_grad",
    "_grain",
    "_h",
    "_h_",
    "_ha",
    "_hbar_",
    "_hp",
    "_inH2O",
    "_inHg",
    "_inch"       ,
    "_j",
    "_k_",
    "_kg",
    "_kip",
    "_knot",
    "_kph",
    "_kq_",
    "_l",
    "_lam",
    "_lambda0_",
    "_lambdac_",
    "_lb",
    "_lbf",
    "_lbmol",
    "_lbt",
    "_lep",
    "_liqpt",
    "_lyr",
    "_m",
    "_mEarth_",
    "_mSun_",
    "_me_",
    "_mho",
    "_mi",
    "_miUS",
    "_mil",
    "_mile",
    "_mille",
    "_min",
    "_mmHg",
    "_mn",
    "_mol",
    "_molK",
    "_mp_",
    "_mph",
    "_mpme_",
    "_mu0_",
    "_muB_",
    "_muN_",
    "_nmi",
    "_oz",
    "_ozUK",
    "_ozfl",
    "_ozt",
    "_pc",
    "_pdl",
    "_phi_",
    "_pk",
    "_psi",
    "_pt",
    "_ptUK",
    "_qe_",
    "_qepsilon0_",
    "_qme_",
    "_qt",
    "_rad",
    "_rd",
    "_rem",
    "_rev",
    "_rod",
    "_rpm",
    "_s",
    "_sb",
    "_sd_",
    "_sigma_",
    "_slug",
    "_st",
    "_syr_",
    "_t",
    "_tbsp",
    "_tec",
    "_tep",
    "_tepC",
    "_tepcC",
    "_tepgC",
    "_tex",
    "_therm",
    "_toe",
    "_ton",
    "_tonUK",
    "_tr",
    "_tsp",
    "_u",
    "_yd",
    "_yr",
#ifndef VISUALC
    "_\u03BC"
    // "_µ"
#endif
  };

  const char * const * const unitname_tab_end=unitname_tab+unitptr_tab_length;
#ifndef NO_PHYSICAL_CONSTANTS
  gen _kg_unit(mksa_register("_kg",&__kg_unit));
  gen _m_unit(mksa_register("_m",&__m_unit));
  gen _s_unit(mksa_register("_s",&__s_unit));
  gen _A_unit(mksa_register("_A",&__A_unit));
  gen _K_unit(mksa_register("_K",&__K_unit)); // Kelvin
  gen _mol_unit(mksa_register("_mol",&__mol_unit)); // mol
  gen _cd_unit(mksa_register("_cd",&__cd_unit)); // candela
  gen _E_unit(mksa_register("_E",&__E_unit)); // euro

#ifndef STATIC_BUILTIN_LEXER_FUNCTIONS
  gen _C_unit(mksa_register("_C",&__C_unit));
  gen _F_unit(mksa_register("_F",&__F_unit));
  gen _Gy_unit(mksa_register("_Gy",&__Gy_unit));
  gen _H_unit(mksa_register("_H",&__H_unit));
  gen _Hz_unit(mksa_register("_Hz",&__Hz_unit));
  gen _J_unit(mksa_register("_J",&__J_unit));
  gen _mho_unit(mksa_register("_mho",&__mho_unit));
  gen _N_unit(mksa_register("_N",&__N_unit));
  gen _Ohm_unit(mksa_register("_Ohm",&__Ohm_unit));
  gen _Pa_unit(mksa_register("_Pa",&__Pa_unit));
  gen _rad_unit(mksa_register("_rad",&__rad_unit)); // radian
  gen _S_unit(mksa_register("_S",&__S_unit));
  gen _Sv_unit(mksa_register("_Sv",&__Sv_unit));
  gen _T_unit(mksa_register("_T",&__T_unit));
  gen _V_unit(mksa_register("_V",&__V_unit));
  gen _W_unit(mksa_register("_W",&__W_unit));
  gen _Wb_unit(mksa_register("_Wb",&__Wb_unit));
  gen _l_unit(mksa_register("_l",&__l_unit));
  gen _molK_unit(mksa_register("_molK",&__molK_unit));

  gen _Bq_unit(mksa_register("_Bq",&__Bq_unit));
  gen _L_unit(mksa_register("_L",&__L_unit));
  // other metric units in m,kg,s,A
  gen _st_unit(mksa_register("_st",&__st_unit));
  // useful non metric units
  gen _a_unit(mksa_register("_a",&__a_unit));
  gen _acre_unit(mksa_register("_acre",&__acre_unit));
  gen _arcmin_unit(mksa_register("_arcmin",&__arcmin_unit));
  gen _arcs_unit(mksa_register("_arcs",&__arcs_unit));
  gen _atm_unit(mksa_register("_atm",&__atm_unit));
  gen _au_unit(mksa_register("_au",&__au_unit));
  gen _Angstrom_unit(mksa_register("_Angstrom",&__Angstrom_unit));
  gen _micron_unit(mksa_register("_\u03BC",&__micron_unit));
  //gen _micron_unit(mksa_register("_µ",&__micron_unit));
  gen _b_unit(mksa_register("_b",&__b_unit));
  gen _bar_unit(mksa_register("_bar",&__bar_unit));
  gen _bbl_unit(mksa_register("_bbl",&__bbl_unit));
  gen _buUS(mksa_register("_buUS",&__buUS));
  gen _bu(mksa_register("_bu",&__bu));
  gen _Btu_unit(mksa_register("_Btu",&__Btu_unit));
  gen _cal_unit(mksa_register("_cal",&__cal_unit));
  gen _chain_unit(mksa_register("_chain",&__chain_unit));
  gen _Curie_unit(mksa_register("_Curie",&__Curie_unit));
  gen _ct_unit(mksa_register("_ct",&__ct_unit));
  gen _deg_unit(mksa_register("_deg",&__deg_unit));
  gen _d_unit(mksa_register("_d",&__d_unit));
  gen _dB_unit(mksa_register("_dB",&__dB_unit));
  gen _dyn_unit(mksa_register("_dyn",&__dyn_unit));
  gen _erg_unit(mksa_register("_erg",&__erg_unit));
  gen _eV_unit(mksa_register("_eV",&__eV_unit));
  // gen _degreeF_unit(mksa_register("_degreeF",&__degreeF_unit));
  gen _Rankine_unit(mksa_register("_Rankine",&__Rankine_unit));
  gen _fath_unit(mksa_register("_fath",&__fath_unit));
  gen _fm_unit(mksa_register("_fm",&__fm_unit));
  gen _fbm_unit(mksa_register("_fbm",&__fbm_unit));
  // gen _fc_unit(mksa_register("_fc",&__fc_unit));
  gen _Fdy_unit(mksa_register("_Fdy",&__Fdy_unit));
  gen _fermi_unit(mksa_register("_fermi",&__fermi_unit));
  gen _flam_unit(mksa_register("_flam",&__flam_unit));
  gen _ft_unit(mksa_register("_ft",&__ft_unit));
  gen _ftUS_unit(mksa_register("_ftUS",&__ftUS_unit));
  gen _Gal(mksa_register("_Gal",&__Gal));
  gen _g_unit(mksa_register("_g",&__g_unit));
  gen _galUS_unit(mksa_register("_galUS",&__galUS_unit));
  gen _galC_unit(mksa_register("_galC",&__galC_unit));
  gen _galUK_unit(mksa_register("_galUK",&__galUK_unit));
  gen _gf_unit(mksa_register("_gf",&__gf_unit));
  gen _gmol_unit(mksa_register("_gmol",&__gmol_unit));
  gen _grad_unit(mksa_register("_grad",&__grad_unit));
  gen _gon_unit(mksa_register("_gon",&__gon_unit));
  gen _grain_unit(mksa_register("_grain",&__grain_unit));
  gen _ha_unit(mksa_register("_ha",&__ha_unit));
  gen _h_unit(mksa_register("_h",&__h_unit));
  gen _hp_unit(mksa_register("_hp",&__hp_unit));
  gen _in_unit(mksa_register("_inch",&__in_unit));
  gen _inHg_unit(mksa_register("_inHg",&__inHg_unit));
  gen _inH2O_unit(mksa_register("_inH2O",&__inH2O_unit));
  gen _j_unit(mksa_register("_j",&__j_unit));
  gen _FF_unit(mksa_register("_FF",&__FF_unit));
  gen _kip_unit(mksa_register("_kip",&__kip_unit));
  gen _knot_unit(mksa_register("_knot",&__knot_unit));
  gen _kph_unit(mksa_register("_kph",&__kph_unit));
  gen _lam_unit(mksa_register("_lam",&__lam_unit));
  gen _lb_unit(mksa_register("_lb",&__lb_unit));
  gen _lbf_unit(mksa_register("_lbf",&__lbf_unit));
  gen _lbmol_unit(mksa_register("_lbmol",&__lbmol_unit));
  gen _lbt_unit(mksa_register("_lbt",&__lbt_unit));
  gen _lyr_unit(mksa_register("_lyr",&__lyr_unit));
  gen _mi_unit(mksa_register("_mi",&__mi_unit));
  gen _mil_unit(mksa_register("_mil",&__mil_unit));
  gen _mile_unit(mksa_register("_mile",&__mile_unit));
  gen _mille_unit(mksa_register("_mille",&__mille_unit));
  gen _mn_unit(mksa_register("_mn",&__mn_unit));
  gen _miUS_unit(mksa_register("_miUS",&__miUS_unit));
  gen _mmHg_unit(mksa_register("_mmHg",&__mmHg_unit));
  gen _mph_unit(mksa_register("_mph",&__mph_unit));
  gen _nmi_unit(mksa_register("_nmi",&__nmi_unit));
  gen _oz_unit(mksa_register("_oz",&__oz_unit));
  gen _ozfl_unit(mksa_register("_ozfl",&__ozfl_unit));
  gen _ozt_unit(mksa_register("_ozt",&__ozt_unit));
  gen _ozUK_unit(mksa_register("_ozUK",&__ozUK_unit));
  gen _P_unit(mksa_register("_P",&__P_unit));
  gen _pc_unit(mksa_register("_pc",&__pc_unit));
  gen _pdl_unit(mksa_register("_pdl",&__pdl_unit));
  gen _pk_unit(mksa_register("_pk",&__pk_unit));
  gen _psi_unit(mksa_register("_psi",&__psi_unit));
  gen _pt_unit(mksa_register("_pt",&__pt_unit));
  gen _ptUK_unit(mksa_register("_ptUK",&__ptUK_unit));
  gen _liqpt_unit(mksa_register("_liqpt",&__liqpt_unit));
  gen _qt_unit(mksa_register("_qt",&__qt_unit));
  gen _R_unit(mksa_register("_R",&__R_unit));
  gen _rd_unit(mksa_register("_rd",&__rd_unit));
  gen _rod_unit(mksa_register("_rod",&__rod_unit));
  gen _rem_unit(mksa_register("_rem",&__rem_unit));
  gen _rpm_unit(mksa_register("_rpm",&__rpm_unit));
  gen _sb_unit(mksa_register("_sb",&__sb_unit));
  gen _slug_unit(mksa_register("_slug",&__slug_unit));
  gen _St_unit(mksa_register("_St",&__St_unit));
  gen _t_unit(mksa_register("_t",&__t_unit));
  gen _tbsp_unit(mksa_register("_tbsp",&__tbsp_unit));
  gen _tex(mksa_register("_tex",&__tex));
  gen _therm_unit(mksa_register("_therm",&__therm_unit));
  gen _ton_unit(mksa_register("_ton",&__ton_unit));
  gen _tonUK_unit(mksa_register("_tonUK",&__tonUK_unit));
  gen _Torr_unit(mksa_register("_Torr",&__Torr_unit));
  gen _tr_unit(mksa_register("_tr",&__tr_unit)); // radian
  gen _u_unit(mksa_register("_u",&__u_unit));
  gen _yd_unit(mksa_register("_yd",&__yd_unit));
  gen _yr_unit(mksa_register("_yr",&__yr_unit));

  // Some hydrocarbur energy equivalent
  // tep=tonne equivalent petrole, lep litre equivalent petrole
  // toe=(metric) ton of oil equivalent
  // bblep = baril equivalent petrole, boe=baril of oil equivalent
  gen _tep_unit(mksa_register("_tep",&__tep_unit));
  gen _toe_unit(mksa_register("_toe",&__toe_unit));
  gen _cf_unit(mksa_register("_cf",&__cf_unit));
  gen _tec_unit(mksa_register("_tec",&__tec_unit));
  gen _lep_unit(mksa_register("_lep",&__lep_unit));
  gen _bblep_unit(mksa_register("_bblep",&__bblep_unit));
  gen _boe_unit(mksa_register("_boe",&__boe_unit));
  gen _Wh_unit(mksa_register("_Wh",&__Wh_unit));
  // Equivalent Carbon for 1 tep, oil, gas, coal
  gen _tepC_unit(mksa_register("_tepC",&__tepC_unit));
  gen _tepgC_unit(mksa_register("_tepgC",&__tepgC_unit));
  gen _tepcC_unit(mksa_register("_tepcC",&__tepcC_unit));
  // mean PRG for HFC in kg C unit
  gen _HFCC_unit(mksa_register("_HFCC",&__HFCC_unit));
#endif // ndef STATIC_BUILTIN_LEXER_FUNCTIONS
#endif

  static vecteur mksa_unit2vecteur(const mksa_unit * tmp){
    vecteur v;
    if (tmp->K==0 && tmp->mol==0 && tmp->cd==0){
      if (tmp->m==0 && tmp->kg==0 && tmp->s==0 && tmp->A==0 && tmp->E==0){
	v.push_back(tmp->coeff);
      }
      else {
	v.reserve(5);
	v.push_back(tmp->coeff);
	v.push_back(tmp->m);
	v.push_back(tmp->kg);
	v.push_back(tmp->s);
	v.push_back(tmp->A);
      }
    }
    else {
      v.reserve(9);
      v.push_back(tmp->coeff);
      v.push_back(tmp->m);
      v.push_back(tmp->kg);
      v.push_back(tmp->s);
      v.push_back(tmp->A);
      v.push_back(tmp->K);
      v.push_back(tmp->mol);
      v.push_back(tmp->cd);
      v.push_back(tmp->E);
    }
    return v;
  }

  static vecteur mksa_unit2vecteur(const string & s){
    for (int i=0;i<sizeof(tab_unit_rom)/sizeof(gen *);++i){
      if (!strcmp(s.c_str(),tab_unit_rom[i]->_IDNTptr->id_name))
	return mksa_unit2vecteur(unitptr_tab[i]);
    }
    return mksa_unit2vecteur(unit_conversion_map()[s.substr(1,s.size()-1).c_str()]);
  }

  struct mksa_tri3 {
    mksa_tri3() {}
    bool operator ()(const char * a,const char * b){
      return strcmp(a,b)<0;
    }
  };

  // return a vector of powers in MKSA system
  vecteur mksa_convert(const identificateur & g,GIAC_CONTEXT){
    string s=g.print(contextptr);
    // Find prefix in unit
    int exposant=0;
    int l=int(s.size());
#ifdef USTL
    ustl::pair<const char * const * const,const char * const * const> pp=ustl::equal_range(unitname_tab,unitname_tab_end,s.c_str(),mksa_tri3());
#else
    std::pair<const char * const * const,const char * const * const> pp=equal_range(unitname_tab,unitname_tab_end,s.c_str(),mksa_tri3());
#endif
    if (pp.first!=pp.second && pp.second!=unitname_tab_end)
      mksa_register_unit(*pp.first,unitptr_tab[pp.first-unitname_tab]);
    if (l>1 && s[0]=='_'){
      --l;
      s=s.substr(1,l);
    }
    else
      return makevecteur(g);
    gen res=plus_one;
#ifdef USTL
    ustl::map<const char *, const mksa_unit *,ltstr>::const_iterator it=unit_conversion_map().find(s.c_str()),itend=unit_conversion_map().end();
#else
    std::map<const char *, const mksa_unit *,ltstr>::const_iterator it=unit_conversion_map().find(s.c_str()),itend=unit_conversion_map().end();
#endif
    int nchar=1;
    if (it==itend && l>1){
      switch (s[0]){
      case 'Y':
	exposant=24;
	break;
      case 'Z':
	exposant=21;
	break;
      case 'E':
	exposant=18;
	break;
      case 'P':
	exposant=15;
	break;
      case 'T':
	exposant=12;
	break;
      case 'G':
	exposant=9;
	break;
      case 'M':
	exposant=6;
	break;
      case 'K': case 'k':
	exposant=3;
	break;
      case 'H': case 'h':
	exposant=2;
	break;
      case 'D':
	exposant=1;
	break;
      case 'd':
	exposant=-1;
	break;
      case 'c':
	exposant=-2;
	break;
      case 'm':
	exposant=-3;
	break;
      case char(0xc2): // micro 
	if (l>2 && s[1]==char(0xB5)) {
	  nchar=2;
	  exposant=-6;
	}	
	break;
      case 'n':
	exposant=-9;
	break;
      case 'p':
	exposant=-12;
	break;
      case 'f':
	exposant=-15;
	break;
      case 'a':
	exposant=-18;
	break;
      case 'z':
	exposant=-21;
	break;
      case 'y':
	exposant=-24;
	break;
      }
    }
    if (exposant!=0){
      s=s.substr(nchar,l-nchar);
      res=std::pow(10.0,double(exposant));
#ifdef USTL
      ustl::pair<const char * const * const,const char * const * const> pp=ustl::equal_range(unitname_tab,unitname_tab_end,("_"+s).c_str(),mksa_tri3());
#else
      std::pair<const char * const * const,const char * const * const> pp=equal_range(unitname_tab,unitname_tab_end,("_"+s).c_str(),mksa_tri3());
#endif
      if (pp.first!=pp.second && pp.second!=unitname_tab_end)
	mksa_register_unit(*pp.first,unitptr_tab[pp.first-unitname_tab]);
      it=unit_conversion_map().find(s.c_str());
    }
    if (it==itend)
      return makevecteur(operator_times(res,find_or_make_symbol("_"+s,false,contextptr),contextptr));
    vecteur v=mksa_unit2vecteur(it->second);
    v[0]=operator_times(res,v[0],contextptr);
    return v;
  }

  vecteur mksa_convert(const gen & g,GIAC_CONTEXT){
    if (g.type==_IDNT)
      return mksa_convert(*g._IDNTptr,contextptr);
    if (g.type!=_SYMB)
      return makevecteur(g);
    if (g.is_symb_of_sommet(at_unit)){
      vecteur & v=*g._SYMBptr->feuille._VECTptr;
      vecteur res0=mksa_convert(v[1],contextptr);
      vecteur res1=mksa_convert(v[0],contextptr);
      vecteur res=addvecteur(res0,res1);
      res.front()=operator_times(res0.front(),res1.front(),contextptr);
      return res;
    }
    if (g._SYMBptr->sommet==at_inv){
      vecteur res(mksa_convert(g._SYMBptr->feuille,contextptr));
      res[0]=inv(res[0],contextptr);
      int s=int(res.size());
      for (int i=1;i<s;++i)
	res[i]=-res[i];
      return res;
    }
    if (g._SYMBptr->sommet==at_pow){
      gen & f=g._SYMBptr->feuille;
      if (f.type!=_VECT||f._VECTptr->size()!=2)
	return vecteur(1,gensizeerr(contextptr));
      vecteur res(mksa_convert(f._VECTptr->front(),contextptr));
      gen e=f._VECTptr->back();
      res[0]=pow(res[0],e,contextptr);
      int s=int(res.size());
      for (int i=1;i<s;++i)
	res[i]=operator_times(e,res[i],contextptr);
      return res;
    }
    if (g._SYMBptr->sommet==at_prod){
      gen & f=g._SYMBptr->feuille;
      if (f.type!=_VECT)
	return mksa_convert(f,contextptr);
      vecteur & v=*f._VECTptr;
      vecteur res(makevecteur(plus_one));
      const_iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it){
	vecteur tmp(mksa_convert(*it,contextptr));
	res[0]=operator_times(res[0],tmp[0],contextptr);
	iterateur it=res.begin()+1,itend=res.end(),jt=tmp.begin()+1,jtend=tmp.end();
	for (;it!=itend && jt!=jtend;++it,++jt)
	  *it=*it+*jt;
	for (;jt!=jtend;++jt)
	  res.push_back(*jt);
      }
      return res;
    }
    return makevecteur(g);
  }

  gen unitpow(const gen & g,const gen & exponent_){
    gen exponent=evalf_double(exponent_,1,context0);
    if (exponent.type!=_DOUBLE_)
      return gensizeerr(gettext("Invalid unit exponent")+exponent.print());
    if (absdouble(exponent._DOUBLE_val)<1e-6)
      return plus_one;
    if (is_one(exponent))
      return g;
    return symbolic(at_pow,gen(makevecteur(g,exponent),_SEQ__VECT));
  }
  gen mksa_reduce(const gen & g,GIAC_CONTEXT){
    if (g.type==_VECT)
      return apply(g,mksa_reduce,contextptr);
    vecteur v(mksa_convert(g,contextptr));
    if (is_undef(v)) return v;
    gen res1=v[0];
    gen res=plus_one;
    int s=int(v.size());
    if (s>2)
      res = res *unitpow(_kg_unit,v[2]);
    if (s>1)
      res = res *unitpow(_m_unit,v[1]);
    if (s>3)
      res = res *unitpow(_s_unit,v[3]);
    if (s>4)
      res = res * unitpow(_A_unit,v[4]);
    if (s>5)
      res = res * unitpow(_K_unit,v[5]);
    if (s>6)
      res = res * unitpow(_mol_unit,v[6]);
    if (s>7)
      res = res * unitpow(_cd_unit,v[7]);
    if (s>8)
      res = res * unitpow(_E_unit,v[8]);
    if (is_one(res))
      return res1;
    else
      return symbolic(at_unit,makevecteur(res1,res));
  }
  static const char _mksa_s []="mksa";
  static define_unary_function_eval (__mksa,&mksa_reduce,_mksa_s);
  define_unary_function_ptr5( at_mksa ,alias_at_mksa,&__mksa,0,true);
  
  gen _ufactor(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (g.type==_VECT && g.subtype==_SEQ__VECT && g._VECTptr->size()==2){
      vecteur & v=*g._VECTptr;
      return v.back()*mksa_reduce(v.front()/v.back(),contextptr);
    }
    return gensizeerr(contextptr);
  }
  static const char _ufactor_s []="ufactor";
  static define_unary_function_eval (__ufactor,&_ufactor,_ufactor_s);
  define_unary_function_ptr5( at_ufactor ,alias_at_ufactor,&__ufactor,0,true);
  
  gen _usimplify(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (g.type==_VECT)
      return apply(g,_usimplify,contextptr);
    if (!g.is_symb_of_sommet(at_unit))
      return g;
    vecteur v=mksa_convert(g,contextptr);
    if (is_undef(v)) return v;
    gen res1=v[0];
    int s=int(v.size());
    for (int i=s;i<5;++i)
      v.push_back(zero);
    for (;s>0;--s){
      if (!is_zero(v[s-1]))
	break;
    }
    if (s>5)
      return g;
    // look first if it's a mksa
    int pos=-1;
    for (int i=1;i<5;++i){
      if (v[i]==zero)
	continue;
      if (pos){
	pos=0;
	break;
      }
      pos=i;
    }
    if (pos==-1)
      return v[0];
    if (pos)
      return mksa_reduce(g,contextptr);
    v[0]=plus_one;
    const gen ** it=tab_usual_units,**itend=it+sizeof(tab_usual_units)/sizeof(const gen *);
    // const_iterateur it=usual_units().begin(),itend=usual_units().end();
    for (;it!=itend;++it){
      string s=(*it)->print(contextptr);
      vecteur tmp=mksa_unit2vecteur(s);
      for (int i=tmp.size();i<5;++i)
	tmp.push_back(zero);
      if (tmp==v)
	return _ufactor(gen(makevecteur(g,symbolic(at_unit,makevecteur(1,**it))),_SEQ__VECT),contextptr);
    }
    // count non-zero in v, if ==2 return mksa
    int count=0;
    const_iterateur jt=v.begin()+1,jtend=v.end();
    for (;jt!=jtend;++jt){
      if (!is_zero(*jt))
	++count;
    }
    if (count<=2) 
      return mksa_reduce(g,contextptr);
    // it=usual_units().begin(); itend=usual_units().end();
    it=tab_usual_units;
    for (;it!=itend;++it){
      string s=(*it)->print(contextptr);
      gen tmp=mksa_unit2vecteur(s);
      vecteur w(*tmp._VECTptr);
      for (int j=0;j<2;j++){
	vecteur vw;
	if (j)
	  vw=addvecteur(v,w);
	else
	  vw=subvecteur(v,w);
	for (int i=1;i<5;++i){
	  if (is_greater(1e-6,abs(vw[i],contextptr),contextptr))
	    continue;
	  if (pos){
	    pos=0;
	    break;
	  }
	  pos=i;
	}
	if (pos){
	  if (j)
	    return _ufactor(gen(makevecteur(g,symbolic(at_unit,makevecteur(1,unitpow(**it,-1)))),_SEQ__VECT),contextptr);
	  else
	    return _ufactor(gen(makevecteur(g,symbolic(at_unit,makevecteur(1,**it))),_SEQ__VECT),contextptr);
	}
      }
    }
    return g;
  }
  static const char _usimplify_s []="usimplify";
  static define_unary_function_eval (__usimplify,&_usimplify,_usimplify_s);
  define_unary_function_ptr5( at_usimplify ,alias_at_usimplify,&__usimplify,0,true);
  
  gen symb_unit(const gen & a,const gen & b,GIAC_CONTEXT){
    if (b==at_min)
      return symb_unit(a,gen("mn",contextptr),contextptr);
    // Add a _ to all identifiers in b
    if (!lop(b,at_of).empty())
      return gensizeerr(contextptr);
    vecteur v(lidnt(b)); // was lvar(b), changed because 1_(km/s) did not work
    for (unsigned i=0;i<v.size();++i){
      if (v[i].type!=_IDNT)
	return gensizeerr(contextptr); // bad unit
    }
    vecteur w(v);
    iterateur it=w.begin(),itend=w.end();
    for (;it!=itend;++it){
      find_or_make_symbol("_"+it->print(contextptr),*it,0,false,contextptr);
    }
    return symbolic(at_unit,makevecteur(a,subst(b,v,w,true,contextptr)));
  }
  string printasunit(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type!=_VECT || feuille._VECTptr->size()!=2)
      return gettext("printasunit error");
    vecteur & v=*feuille._VECTptr;
    vecteur v1(lidnt(v[1]));
    vecteur w(v1);
    iterateur it=w.begin(),itend=w.end();
    for (;it!=itend;++it){
      string s;
      s=it->print(contextptr);
      if (!s.empty() && s[0]=='_')
	s=s.substr(1,s.size()-1);
      *it=identificateur(s);//find_or_make_symbol(s,*it,0,false,contextptr);
    }
    string tmp(subst(v[1],v1,w,true,contextptr).print(contextptr));
    if (tmp[0]=='c' || (v[1].type==_SYMB 
			// && !v[1].is_symb_of_sommet(at_pow)
			) )
      tmp="_("+tmp+")";
    else
      tmp="_"+tmp;
    if (v[0].type<_POLY || v[0].type==_FLOAT_)
      return v[0].print(contextptr)+tmp;
    else
      return "("+v[0].print(contextptr)+")"+tmp;
  }
  static gen unit(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    return symbolic(at_unit,g);
  }
  static const char _unit_s []="_";
  static define_unary_function_eval2_index (112,__unit,&unit,_unit_s,&printasunit);
  define_unary_function_ptr( at_unit ,alias_at_unit ,&__unit);

#ifndef FXCG
  const unary_function_ptr * binary_op_tab(){
    static const unary_function_ptr binary_op_tab_ptr []={*at_plus,*at_prod,*at_pow,*at_and,*at_ou,*at_xor,*at_different,*at_same,*at_equal,*at_unit,*at_compose,*at_composepow,*at_deuxpoints,*at_tilocal,*at_pointprod,*at_pointdivision,*at_pointpow,*at_division,*at_normalmod,*at_minus,*at_intersect,*at_union,*at_interval,*at_inferieur_egal,*at_inferieur_strict,*at_superieur_egal,*at_superieur_strict,*at_equal2,0};
    return binary_op_tab_ptr;
  }
  // unary_function_ptr binary_op_tab[]={at_and,at_ou,at_different,at_same,0};
#endif
  
  // Physical constants -> in input_lexer.ll
#if 0 //ndef NO_PHYSICAL_CONSTANTS
  identificateur _cst_hbar("_hbar_",symbolic(at_unit,makevecteur(1.05457266e-34,_J_unit*_s_unit)));
  gen cst_hbar(_cst_hbar);
  identificateur _cst_clightspeed("_c_",symbolic(at_unit,makevecteur(299792458,_m_unit/_s_unit)));
  gen cst_clightspeed(_cst_clightspeed);
  identificateur _cst_ga("_g_",symbolic(at_unit,makevecteur(9.80665,_m_unit*unitpow(_s_unit,-2))));
  gen cst_ga(_cst_ga);
  identificateur _cst_IO("_IO_",symbolic(at_unit,makevecteur(1e-12,_W_unit*unitpow(_m_unit,-2))));
  gen cst_IO(_cst_IO);
  // gen cst_IO("_io",context0); //  IO 1e-12W/m^2
  identificateur _cst_epsilonox("_epsilonox_",3.9);
  gen cst_epsilonox(_cst_epsilonox); // 3.9
  identificateur _cst_epsilonsi("_epsilonsi_",11.9);
  gen cst_epsilonsi(_cst_epsilonsi); // 11.9
  identificateur _cst_qepsilon0("_qepsilon0_",symbolic(at_unit,makevecteur(1.4185979e-30,_F_unit*_C_unit/_m_unit)));
  gen cst_qepsilon0(_cst_qepsilon0); // qeps0 1.4185979e-30 F*C/m
  identificateur _cst_epsilon0q("_epsilon0q_",symbolic(at_unit,makevecteur(55263469.6,_F_unit/(_m_unit*_C_unit))));
  gen cst_epsilon0q(_cst_epsilon0q); // eps0q 55263469.6 F/(m*C)
  identificateur _cst_kq("_kq_",symbolic(at_unit,makevecteur(8.617386e-5,_J_unit/(_K_unit*_C_unit))));
  gen cst_kq(_cst_kq); // kq 8.617386e-5 J/(K*C)
  identificateur _cst_c3("_c3_",symbolic(at_unit,makevecteur(.002897756,_m_unit*_K_unit)));
  gen cst_c3(_cst_c3); // c3 .002897756m*K
  identificateur _cst_lambdac("_lambdac_",symbolic(at_unit,makevecteur( 0.00242631058e-9,_m_unit)));
  gen cst_lambdac(_cst_lambdac); // lambdac 0.00242631058 nm
  identificateur _cst_f0("_f0_",symbolic(at_unit,makevecteur(2.4179883e14,_Hz_unit)));
  gen cst_f0(_cst_f0); //  f0 2.4179883e14Hz
  identificateur _cst_lambda0("_lambda0_",symbolic(at_unit,makevecteur(1239.8425e-9,_m_unit)));
  gen cst_lambda0(_cst_lambda0); // lambda0 1239.8425_nm
  identificateur _cst_muN("_muN_",symbolic(at_unit,makevecteur(5.0507866e-27,_J_unit/_T_unit)));
  gen cst_muN(_cst_muN); // muN 5.0507866e-27_J/T
  identificateur _cst_muB("_muB_",symbolic(at_unit,makevecteur( 9.2740154e-24,_J_unit/_T_unit)));
  gen cst_muB(_cst_muB); // muB 9.2740154e-24 J/T
  identificateur _cst_a0("_a0_",symbolic(at_unit,makevecteur(.0529177249e-9,_m_unit)));
  gen cst_a0(_cst_a0); // a0 .0529177249_nm
  identificateur _cst_Rinfinity("_Rinfinity_",symbolic(at_unit,makevecteur(10973731.534,unitpow(_m_unit,-1))));
  gen cst_Rinfinity(_cst_Rinfinity); // Rinf 10973731.534 m^-1
  identificateur _cst_Faraday("_F_",symbolic(at_unit,makevecteur(96485.309,_C_unit/_mol_unit)));
  gen cst_Faraday(_cst_Faraday); // F 96485.309 C/gmol
  identificateur _cst_phi("_phi_",symbolic(at_unit,makevecteur(2.06783461e-15,_Wb_unit)));
  gen cst_phi(_cst_phi); // phi 2.06783461e-15 Wb
  identificateur _cst_alpha("_alpha_",7.29735308e-3);
  gen cst_alpha(_cst_alpha); // alpha 7.29735308e-3
  identificateur _cst_mpme("_mpme_",1836.152701);
  gen cst_mpme(_cst_mpme); // mpme 1836.152701
  identificateur _cst_mp("_mp_",symbolic(at_unit,makevecteur(1.6726231e-27,_kg_unit)));
  gen cst_mp(_cst_mp); // mp 1.6726231e-27 kg
  identificateur _cst_qme("_qme_",symbolic(at_unit,makevecteur(1.75881962e11,_C_unit/_kg_unit)));
  gen cst_qme(_cst_qme); // qme 175881962000 C/kg
  identificateur _cst_me("_me_",symbolic(at_unit,makevecteur(9.1093897e-31,_kg_unit)));
  gen cst_me(_cst_me); // me 9.1093897e-31 kg
  identificateur _cst_qe("_qe_",symbolic(at_unit,makevecteur(1.60217733e-19,_C_unit)));
  gen cst_qe(_cst_qe); // q 1.60217733e-19 C
  identificateur _cst_hPlanck("_h_",symbolic(at_unit,makevecteur(6.6260755e-34,_J_unit*_s_unit)));
  gen cst_hPlanck(_cst_hPlanck); //  h 6.6260755e-34 Js
  identificateur _cst_G("_G_",symbolic(at_unit,makevecteur(6.67408e-11,unitpow(_m_unit,3)*unitpow(_s_unit,-2)*unitpow(_kg_unit,-1))));
  gen cst_G(_cst_G); // G 6.67408e-11m^3/s^2kg
  identificateur _cst_mu0("_mu0_",symbolic(at_unit,makevecteur(1.25663706144e-6,_H_unit/_m_unit)));
  gen cst_mu0(_cst_mu0); // mu0 1.25663706144e-6 H/m
  identificateur _cst_epsilon0("_epsilon0_",symbolic(at_unit,makevecteur(8.85418781761e-12,_F_unit/_m_unit)));
  gen cst_epsilon0(_cst_epsilon0); // eps0 8.85418781761e-12 F/m
  identificateur _cst_sigma("_sigma_",symbolic(at_unit,makevecteur( 5.67051e-8,_W_unit*unitpow(_m_unit,-2)*unitpow(_K_unit,-4))));
  gen cst_sigma(_cst_sigma); // sigma 5.67051e-8 W/m^2*K^4
  identificateur _cst_StdP("_StdP_",symbolic(at_unit,makevecteur(101325.0,_Pa_unit)));
  gen cst_StdP(_cst_StdP); // StdP 101.325_kPa
  identificateur _cst_StdT("_StdT_",symbolic(at_unit,makevecteur(273.15,_K_unit)));
  gen cst_StdT(_cst_StdT); // StdT 273.15_K
  identificateur _cst_Rydberg("_R_",symbolic(at_unit,makevecteur(8.31451,_J_unit/_molK_unit)));
  gen cst_Rydberg(_cst_Rydberg); // Rydberg 8.31451_J/(gmol*K)
  identificateur _cst_Vm("_Vm_",symbolic(at_unit,makevecteur(22.4141,_l_unit/_mol_unit)));
  gen cst_Vm(_cst_Vm); // Vm 22.4141_l/gmol
  identificateur _cst_kBoltzmann("_k_",symbolic(at_unit,makevecteur(1.380658e-23,_J_unit/_K_unit)));
  gen cst_kBoltzmann(_cst_kBoltzmann); // k 1.380658e-23 J/K
  identificateur _cst_NA("_NA_",symbolic(at_unit,makevecteur(6.0221367e23,unitpow(_mol_unit,-1))));
  gen cst_NA(_cst_NA); // NA 6.0221367e23 1/gmol
#endif // NO_PHYSICAL_CONSTANTS

  gen maple_root(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return symbolic(at_maple_root,g);
    vecteur & v=*g._VECTptr;
#ifdef KHICAS
    return pow(v[0],inv(v[1],contextptr),contextptr);
#else
    return pow(v[1],inv(v[0],contextptr),contextptr);
#endif
  }
  static const char _maple_root_s []="root";
#if defined RTOS_THREADX || defined NSPIRE
  static define_unary_function_eval(__maple_root,&maple_root,_maple_root_s);
#else
  static const unary_function_eval __maple_root(0,&maple_root,_maple_root_s);
#endif
  define_unary_function_ptr( at_maple_root ,alias_at_maple_root ,&__maple_root);

  gen symb_interrogation(const gen & e1,const gen & e3){
    if (e3.is_symb_of_sommet(at_deuxpoints)){
      gen & f =e3._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2)
	return symb_when(e1,f._VECTptr->front(),f._VECTptr->back());
    }
    return symb_when(e1,e3,undef);
  }

  bool first_ascend_sort(const gen & a,const gen & b){
    gen g=inferieur_strict(a[0],b[0],context0); 
    if (g.type!=_INT_)
      return a[0].islesscomplexthan(b[0]);
    return g.val==1;
  }
  bool first_descend_sort(const gen & a,const gen & b){
    gen g=superieur_strict(a[0],b[0],context0); 
    if (g.type!=_INT_)
      return !a[0].islesscomplexthan(b[0]);
    return g.val==1;
  }

#ifdef NO_UNARY_FUNCTION_COMPOSE
  gen user_operator(const gen & g,GIAC_CONTEXT){
    return gensizeerr(gettext("User operator not available on this architecture"));
  }

#else
  // Create an operator with a given syntax
  vector<unary_function_ptr> user_operator_list;   // GLOBAL VAR
  gen user_operator(const gen & g,GIAC_CONTEXT){
    int nargs=0;
    if (g.type!=_VECT || (nargs=g._VECTptr->size())<2)
      return gensizeerr(contextptr);
    vecteur & v=*g._VECTptr;
    // int s=signed(v.size());
    if (v[0].type!=_STRNG)
      return string2gen(gettext("Operator name must be of type string"),false);
    string & ss=*v[0]._STRNGptr;
    vector<unary_function_ptr>::iterator it=user_operator_list.begin(),itend=user_operator_list.end();
    for (;it!=itend;++it){
      if (it->ptr()->s==ss){
	break;
      }
    }
    if (it!=itend){
      const unary_function_abstract * ptr0=it->ptr();
      const unary_function_user * ptr=dynamic_cast<const unary_function_user *>(ptr0);
      if (!ptr)
	return zero;
      if ( (nargs==3 && ptr->f==v[1] && v[2].type==_INT_ && v[2].subtype==_INT_MUPADOPERATOR && v[2].val==_DELETE_OPERATOR) ||
	   (nargs==2 && v[1].type==_INT_ && v[1].subtype==_INT_MUPADOPERATOR && v[1].val==_DELETE_OPERATOR) ){
	map_charptr_gen::const_iterator i,iend;
	bool ok=true;
	i = lexer_functions().find(v[0]._STRNGptr->c_str());
	iend=lexer_functions().end();
	if (i==iend)
	  ok=false;
	else
	  lexer_functions().erase(v[0]._STRNGptr->c_str());
	user_operator_list.erase(it); 
      }
      return plus_one;
    }
    if (nargs<3)
      return 1;
    if (v[2].type==_INT_){ 
      int token_value=v[2].val;
      unary_function_user * uf;
      if (v[2].subtype==_INT_MUPADOPERATOR){
	switch (v[2].val){
	case _POSTFIX_OPERATOR:
	  uf= new unary_function_user (0,v[1],ss,0,0,0);
	  token_value=T_FACTORIAL; // like factorial
	  break;
	case _PREFIX_OPERATOR:
	  uf=new unary_function_user(0,v[1],ss,0,0,0);
	  token_value=T_NOT; // like not
	  break;
	case _BINARY_OPERATOR:
	  uf = new unary_function_user (0,v[1],ss);
	  token_value=T_FOIS; // like *
	  break;
	default:
	  return zero;
	}
      }
      else 
	// non mupad syntax, v[2] is input_parser.yy token value
	uf = new unary_function_user(0,v[1],ss);
      unary_function_ptr u(uf);
      // cout << symbolic(u,makevecteur(1,2)) << '\n';
      user_operator_list.push_back(u);
      static vector<string> * user_operator_string=0;
      if (!user_operator_string) 
	user_operator_string=new vector<string>;
      int pos=equalposcomp(*user_operator_string,ss);
      if (!pos){
	user_operator_string->push_back(ss);
	pos=user_operator_string->size();
      }
      bool res=lexer_functions_register(u,(*user_operator_string)[pos-1].c_str(),token_value);
      if (res){
#ifdef HAVE_SIGNAL_H_OLD
	if (!child_id)
	  _signal(symb_quote(symbolic(at_user_operator,g)),contextptr);
#endif
	return plus_one;
      }
      user_operator_list.pop_back();
      delete uf;
    }
    return zero;
  }
#endif // NO_UNARY_FUNCTION_COMPOSE
  static const char _user_operator_s []="user_operator";
  static define_unary_function_eval (__user_operator,&user_operator,_user_operator_s);
  define_unary_function_ptr( at_user_operator ,alias_at_user_operator ,&__user_operator);

  gen current_folder_name;

  gen getfold(const gen & g){
    if (is_zero(g))
      return string2gen("main",false);
    return g;
  }

  gen _SetFold(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (!is_zero(g) && g.type!=_IDNT)
      return gensizeerr(contextptr);
    bool ok=is_zero(g);
    if (g.type==_IDNT && g._IDNTptr->value && g._IDNTptr->value->type==_VECT && g._IDNTptr->value->subtype==_FOLDER__VECT)
      ok=true;
    if ( ok || (g.type==_IDNT && g._IDNTptr->id_name && (strcmp(g._IDNTptr->id_name,"main")==0|| strcmp(g._IDNTptr->id_name,"home")) ) ){
      gen res=current_folder_name;
      current_folder_name=g;
#ifdef HAVE_SIGNAL_H_OLD
      if (!child_id)
	_signal(symb_quote(symbolic(at_SetFold,g)),contextptr);
#endif
      return getfold(res);
    }
    return gensizeerr(gettext("Non existent Folder"));
  }
  static const char _SetFold_s []="SetFold";
  static define_unary_function_eval2_quoted (__SetFold,&_SetFold,_SetFold_s,&printastifunction);
  define_unary_function_ptr5( at_SetFold ,alias_at_SetFold,&__SetFold,_QUOTE_ARGUMENTS,T_RETURN); 
  
  static string printaspiecewise(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    // if ( feuille.type!=_VECT || feuille._VECTptr->empty() || abs_calc_mode(contextptr)!=38)
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
#if 0
    vecteur & v = *feuille._VECTptr;
    string res("CASE");
    int s=int(v.size());
    for (int i=0;i<s/2;i++){
      res += " IF ";
      res += v[2*i].print(contextptr);
      res += " THEN ";
      res += printasinnerbloc(v[2*i+1],contextptr);
      res += " END";
    }
    if (s%2){
      res += " DEFAULT ";
      res += printasinnerbloc(v[s-1],contextptr);
    }
    return res+" END";
#endif
  }
  gen _piecewise(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    // evaluate couples of condition/expression, like in a case
#if defined GIAC_HAS_STO_38 || defined NSPIRE || defined NSPIRE_NEWLIB || defined FXCG || defined GIAC_GGB || defined USE_GMP_REPLACEMENTS || defined KHICAS || defined SDL_KHICAS
#else
    // change by L. Marohnić: convert g to piecewise
    gen gg=unquote(g,contextptr);
    if (gg.type!=_VECT || (gg.subtype==_SEQ__VECT && gg._VECTptr->size()==2 &&
        !is_logical(gg._VECTptr->front()) && gg._VECTptr->back().type==_IDNT)) {
      gen e=flatten_piecewise(gg.type==_VECT?gg._VECTptr->front():gg,contextptr);
      gen x=gg.type==_VECT?gg._VECTptr->back():undef;
      if (!is_undef(x) && _eval(x,contextptr).type!=_IDNT)
        return gentypeerr(contextptr);
      vecteur vars=is_undef(x)?*_revlist(_sort(_lname(e,contextptr),contextptr),contextptr)._VECTptr:vecteur(1,x);
      for (const_iterateur it=vars.begin();it!=vars.end();++it) {
        if (it->type!=_IDNT) continue;
        gen p=to_piecewise(e,*it->_IDNTptr,contextptr);
        if (p.is_symb_of_sommet(at_piecewise))
          return p;
      }
      return gg.type==_VECT?gg._VECTptr->front():gg;
    }
#endif
    if (g.type!=_VECT)
      return g;
    vecteur & v =*g._VECTptr;
    int s=int(v.size());
    gen test;
    for (int i=0;i<s/2;++i){
      test=v[2*i];
      test=equaltosame(test.eval(eval_level(contextptr),contextptr)).eval(eval_level(contextptr),contextptr);
      test=test.evalf_double(eval_level(contextptr),contextptr);
      if ( (test.type!=_DOUBLE_) && (test.type!=_CPLX) )
	return symbolic(at_piecewise,g.eval(eval_level(contextptr),contextptr));
      if (is_zero(test))
	continue;
      if (2*i+1<s)
        return v[2*i+1].eval(eval_level(contextptr),contextptr);
      else
        return gensizeerr(gettext("No case applies"));
    }
    if (s%2)
      return v[s-1].eval(eval_level(contextptr),contextptr);
    return undeferr(gettext("No case applies"));
  }
  static const char _piecewise_s []="piecewise";
  static define_unary_function_eval2_quoted (__piecewise,&_piecewise,_piecewise_s,&printaspiecewise);
  define_unary_function_ptr5( at_piecewise ,alias_at_piecewise,&__piecewise,_QUOTE_ARGUMENTS,true);

  static const char _PIECEWISE_s []="PIECEWISE";
  static define_unary_function_eval2_quoted (__PIECEWISE,&_piecewise,_PIECEWISE_s,&printaspiecewise);
  define_unary_function_ptr5( at_PIECEWISE ,alias_at_PIECEWISE,&__PIECEWISE,_QUOTE_ARGUMENTS,true);

  gen _geo2d(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    return g;
  }
  static const char _geo2d_s []="geo2d";
  static define_unary_function_eval (__geo2d,&_geo2d,_geo2d_s);
  define_unary_function_ptr5( at_geo2d ,alias_at_geo2d,&__geo2d,0,true);

  static const char _geo3d_s []="geo3d";
  static define_unary_function_eval (__geo3d,&_geo2d,_geo3d_s);
  define_unary_function_ptr5( at_geo3d ,alias_at_geo3d,&__geo3d,0,true);

  static const char _spreadsheet_s []="spreadsheet";
  static define_unary_function_eval (__spreadsheet,&_geo2d,_spreadsheet_s);
  define_unary_function_ptr5( at_spreadsheet ,alias_at_spreadsheet,&__spreadsheet,0,true);

  std::string print_program_syntax(int maple_mode){
    string logs;
    switch (maple_mode){
    case 0:
      logs="xcas";
      break;
    case 1:
      logs="maple";
      break;
    case 2:
      logs="mupad";
      break;
    case 3:
      logs="ti";
      break;
    default:
      logs=print_INT_(maple_mode);
    }
    return logs;
  }

  gen _threads_allowed(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (is_zero(g))
      threads_allowed=false;
    else
      threads_allowed=true;
    return threads_allowed;
  }
  static const char _threads_allowed_s []="threads_allowed";
  static define_unary_function_eval (__threads_allowed,&_threads_allowed,_threads_allowed_s);
  define_unary_function_ptr5( at_threads_allowed ,alias_at_threads_allowed,&__threads_allowed,0,true);

  gen _mpzclass_allowed(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG &&  g.subtype==-1) return  g;
    if (is_zero(g))
      mpzclass_allowed=false;
    else
      mpzclass_allowed=true;
    return mpzclass_allowed;
  }
  static const char _mpzclass_allowed_s []="mpzclass_allowed";
  static define_unary_function_eval (__mpzclass_allowed,&_mpzclass_allowed,_mpzclass_allowed_s);
  define_unary_function_ptr5( at_mpzclass_allowed ,alias_at_mpzclass_allowed,&__mpzclass_allowed,0,true);

  gen whentopiecewise(const gen & g,GIAC_CONTEXT){
    return symbolic(at_piecewise,g);
  }
  const alias_type when_tab_alias[]={(alias_type)&__when,0};
  const unary_function_ptr * const when_tab=(const unary_function_ptr * const)when_tab_alias;
  const gen_op_context when2piecewise_tab[]={whentopiecewise,0};
  gen when2piecewise(const gen & g,GIAC_CONTEXT){
    return subst(g,when_tab,when2piecewise_tab,false,contextptr);
    /*
    vector< gen_op_context > when2piecewise_v(1,whentopiecewise);
    vector< const unary_function_ptr *> when_v(1,at_when);
    return subst(g,when_v,when2piecewise_v,false,contextptr);
    */
  }

  gen piecewisetowhen(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT)
      return g;
    vecteur v = *g._VECTptr;
    int s=int(v.size());
    if (s==1)
      return gensizeerr(contextptr);
    if (s>=2 && v[0].type==_INT_){
      if (v[0].val)
	return v[1];
      else
	return s==2?bounded_function(contextptr):piecewisetowhen(vecteur(v.begin()+2,v.end()),contextptr);
    }
    if (s==2){
      v.push_back(0); // undef does not work
      return symbolic(at_when,gen(v,_SEQ__VECT));
    }
    if (s==3)
      return symbolic(at_when,g);
    gen tmp=piecewisetowhen(vecteur(v.begin()+2,v.end()),contextptr);
    return symbolic(at_when,gen(makevecteur(v[0],v[1],tmp),_SEQ__VECT));
  }
  const alias_type piecewise_tab_alias[]={alias_at_piecewise,0};
  const unary_function_ptr * const piecewise_tab=(const unary_function_ptr * const)piecewise_tab_alias;
  const gen_op_context piecewise2when_tab[]={piecewisetowhen,0};
  gen piecewise2when(const gen & g,GIAC_CONTEXT){
    return subst(g,piecewise_tab,piecewise2when_tab,false,contextptr);
    /*
    vector< const unary_function_ptr *> piecewise_v(1,at_piecewise);
    vector< gen_op_context > piecewise2when_v(1,piecewisetowhen);
    return subst(g,piecewise_v,piecewise2when_v,false,contextptr);
    */
  }

  gen whentosign(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->size()!=3)
      return gensizeerr(contextptr);
    vecteur v = *g._VECTptr;
    if (v[0].is_symb_of_sommet(at_not))
      return when2sign(makevecteur(v[0]._SYMBptr->feuille,v[2],v[1]),contextptr);
    if (v[0].is_symb_of_sommet(at_and) && v[0]._SYMBptr->feuille.type==_VECT){
      vecteur vand=*v[0]._SYMBptr->feuille._VECTptr;
      if (vand.size()==2)
	return whentosign(makevecteur(vand[0],whentosign(makevecteur(vand[1],v[1],v[2]),contextptr),v[2]),contextptr);
      if (vand.size()>2){
	gen vandlast=vand.back();
	vand.pop_back();
	return whentosign(makevecteur(vandlast,whentosign(makevecteur(symbolic(at_and,vand),v[1],v[2]),contextptr),v[2]),contextptr);
      }
    }
    if (v[0].is_symb_of_sommet(at_ou) && v[0]._SYMBptr->feuille.type==_VECT){
      vecteur vor=*v[0]._SYMBptr->feuille._VECTptr;
      if (vor.size()==2)
	return whentosign(makevecteur(vor[0],v[1],whentosign(makevecteur(vor[1],v[1],v[2]),contextptr)),contextptr);
      if (vor.size()>2){
	gen vorlast=vor.back();
	vor.pop_back();
	return whentosign(makevecteur(vorlast,v[1],whentosign(makevecteur(symbolic(at_and,vor),v[1],v[2]),contextptr)),contextptr);
      }
    }
    if (is_equal(v[0]) || v[0].is_symb_of_sommet(at_same)){
      *logptr(contextptr) << gettext("Assuming false condition ") << v[0].print(contextptr) << '\n';
      return v[2];
    }
    if (v[0].is_symb_of_sommet(at_different)){
      *logptr(contextptr) << gettext("Assuming true condition ") << v[0].print(contextptr) << '\n';
      return v[1];
    }
    bool ok=false;
    if (v[0].is_symb_of_sommet(at_superieur_strict) || v[0].is_symb_of_sommet(at_superieur_egal)){
      v[0]=v[0]._SYMBptr->feuille[0]-v[0]._SYMBptr->feuille[1];
      ok=true;
    }
    if (!ok && (v[0].is_symb_of_sommet(at_inferieur_strict) || v[0].is_symb_of_sommet(at_inferieur_egal)) ){
      v[0]=v[0]._SYMBptr->feuille[1]-v[0]._SYMBptr->feuille[0];
      ok=true;
    }
    if (!ok)
      return gensizeerr(gettext("Unable to handle when condition ")+v[0].print(contextptr));
    return symbolic(at_sign,v[0])*(v[1]-v[2])/2+(v[1]+v[2])/2;
  }
  const gen_op_context when2sign_tab[]={whentosign,0};
  gen when2sign(const gen & g,GIAC_CONTEXT){
    if (equalposcomp(lidnt(g),unsigned_inf))
      *logptr(contextptr) << gettext("when2sign does not work properly with infinities. Replace inf by Inf and run limit after.") << '\n';
    return subst(g,when_tab,when2sign_tab,false,contextptr);
    /*
    vector< gen_op_context > when2sign_v(1,whentosign);
    vector< const unary_function_ptr *> when_v(1,at_when);
    return subst(g,when_v,when2sign_v,false,contextptr);
    */
  }

  gen iftetowhen(const gen & g,GIAC_CONTEXT){
    return symbolic(at_when,g);
  }
  const alias_type ifte_tab_alias[]={(alias_type)&__ifte,0};
  const unary_function_ptr * const ifte_tab=(const unary_function_ptr * const)ifte_tab_alias;
  const gen_op_context ifte2when_tab[]={iftetowhen,0};
  gen ifte2when(const gen & g,GIAC_CONTEXT){
    return subst(g,ifte_tab,ifte2when_tab,false,contextptr);
  }

  // test if m(i) is an array index: that will not be the case if
  // i is an _IDNT or a list of _IDNT
  // 
  bool is_array_index(const gen & m,const gen & i,GIAC_CONTEXT){
    if (i.type==_VECT){
      for (unsigned j=0;j<i._VECTptr->size();++j){
	gen g=(*i._VECTptr)[j];
	if (g.type==_IDNT || g.is_symb_of_sommet(at_equal) || g.is_symb_of_sommet(at_deuxpoints) || g.is_symb_of_sommet(at_sto))
	  continue;
	return true;
      }
    }
    else {
      if (i.type!=_IDNT)
	return true;
    }
    return false;
    // commented otherwise is_array_index inside a program would depend on the global
    // value of m
    //gen mv=eval(m,1,contextptr);
    //return mv.type==_VECT;
  }

  gen _autosimplify(const gen & g,GIAC_CONTEXT){
    if (is_zero(g) && g.type!=_VECT){
      autosimplify("Nop",contextptr);
      return 1;
    }
    if (is_one(g)){
      autosimplify("regroup",contextptr);
      return 1;
    }
    if (g==2){
      autosimplify("simplify",contextptr);
      return 1;
    }
    if (g.type!=_IDNT && g.type!=_FUNC && g.type!=_SYMB)
      return gen(autosimplify(contextptr),contextptr);
    autosimplify(g.print(contextptr),contextptr);
    return 1;
  }
  static const char _autosimplify_s []="autosimplify";
  static define_unary_function_eval (__autosimplify,&_autosimplify,_autosimplify_s);
  define_unary_function_ptr5( at_autosimplify ,alias_at_autosimplify,&__autosimplify,0,true);

  gen _struct_dot(const gen & g,GIAC_CONTEXT){
    if (g.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur w=*g._VECTptr;
    size_t ws=w.size();
    if (ws!=2)
      return gensizeerr(contextptr);
    gen a=w[0],b=w[1],m;
    if (b.type==_IDNT)
      b=eval(b,1,contextptr);
    if (b.type==_SYMB){
      unary_function_ptr u=b._SYMBptr->sommet;
      const gen & f=b._SYMBptr->feuille;
      if (u==at_of && f.type==_VECT && f._VECTptr->size()==2){
	gen s=eval(f._VECTptr->front(),1,contextptr);
	if (s.type==_FUNC)
	  u=*s._FUNCptr;
      }
      if (equalposcomp(plot_sommets,u) || u==at_show || u==at_clear || u==at_scatterplot || u==at_diagrammebatons)
	return eval(b,1,contextptr);
    }
    if (b.type==_FUNC)
      b=symbolic(*b._FUNCptr,gen(vecteur(0),_SEQ__VECT));
    if (a.type==_IDNT){
      gen tmp=eval(a,1,contextptr);
      if (tmp==a){ // try to eval at global level
	tmp=global_eval(tmp,1);
      }
      if (tmp.type==_MAP && b.is_symb_of_sommet(at_of)){
	const gen & f =b._SYMBptr->feuille;
	if (f.type==_VECT && f._VECTptr->size()==2){
	  const vecteur & v =*f._VECTptr;
	  if (v.size()==2 && v.front().type==_IDNT && strcmp(v.front()._IDNTptr->id_name,"update")==0){
	    gen m=eval(v.back(),1,contextptr);
	    if (m.type==_MAP){
	      tmp._MAPptr->insert(m._MAPptr->begin(),m._MAPptr->end());
	      return tmp;
	    }
	  }
	}
      }
      if (tmp.type==_IDNT && (strcmp(tmp._IDNTptr->id_name,"numpy")==0 || strcmp(tmp._IDNTptr->id_name,"pylab")==0 || strcmp(tmp._IDNTptr->id_name,"matplotlib")==0)){
	if (b.type==_SYMB){
	  gen w1=eval(w[1],1,contextptr);
	  // at_equal test added for e.g. matplotlib.xlim(-5,5)
	  if (w1==at_float || w1==at_real || w1.is_symb_of_sommet(at_equal))
	    return w1;
	  tmp=b._SYMBptr->feuille;
	  tmp=eval(tmp,eval_level(contextptr),contextptr);
	  tmp=evalf_double(tmp,1,contextptr);
	  if (b.is_symb_of_sommet(at_dot))
	    return _prod(tmp,contextptr);
	  return b._SYMBptr->sommet(tmp,contextptr);
	}
	gen tmp=eval(b,1,contextptr);
	tmp=evalf_double(tmp,1,contextptr);
	return tmp;
      }
      if (tmp.type==_IDNT && 
	  (strcmp(tmp._IDNTptr->id_name,"math")==0 ||
	   strcmp(tmp._IDNTptr->id_name,"cmath")==0 ||
	   strcmp(tmp._IDNTptr->id_name,"kandinsky")==0)
	  ){
	if (b.type==_SYMB){
	  tmp=eval(b._SYMBptr->feuille,eval_level(contextptr),contextptr);
	  return b._SYMBptr->sommet(tmp,contextptr);
	}
      }
      if (ckmatrix(tmp)){
	if (w[1].type==_IDNT){
	  const char * ch =w[1]._IDNTptr->id_name;
	  if (ch){
	    if (ch[0]=='T')
	      return mtran(*tmp._VECTptr);
	    if (ch[0]=='H')
	      return _trn(tmp,contextptr);
	    if (ch[0]=='I')
	      return minv(*tmp._VECTptr,contextptr);
	  }
	}
	if (w[1].is_symb_of_sommet(at_of))
	  m=tmp;
      }
      else {
	if (tmp.type==_VECT && w[1].type==_IDNT){
	  const char * ch =w[1]._IDNTptr->id_name;
	  if (ch){
	    if (ch[0]=='T')
	      return _tran(tmp,contextptr);
	  }
	}
      }
      if (tmp.type==_VECT && b.type==_SYMB){
	// check tmp size, workaround for progs with l:=[]; l.append(1);
	// where the code would self modify itself
	// disabled, sto should now do a copy of constant lists
	if (b._SYMBptr->sommet==at_append 
	    //&& tmp._VECTptr->size()>=8
	    ){
	  tmp._VECTptr->push_back(eval(b._SYMBptr->feuille,eval_level(contextptr),contextptr));
	  return tmp;
	}
	if ( (b._SYMBptr->sommet==at_suppress || b._SYMBptr->sommet==at_clear) && b._SYMBptr->feuille.type==_VECT && b._SYMBptr->feuille._VECTptr->empty()){
	  tmp._VECTptr->clear();
	  return tmp;
	}
      }
      if (tmp.type==_FUNC)
	a=tmp;
    }
    if (b.type!=_SYMB)
      return _prod(eval(g,eval_level(contextptr),contextptr),contextptr);
    gen f=b;
    unary_function_ptr u=b._SYMBptr->sommet;
    bool cmd=false;
    if (a!=at_random){
      f=b._SYMBptr->feuille;
      if (u==at_of && f.type==_VECT && f._VECTptr->size()==2){
	gen fn=eval(f._VECTptr->front(),1,contextptr);
	if (fn.type==_FUNC){
	  cmd=true;
	  f=f._VECTptr->back();
	  u=*fn._FUNCptr;
	  b=symbolic(u,f);
	}
      }
      vecteur v(1,f);
      if (f.type==_VECT && f.subtype==_SEQ__VECT)
	v=*f._VECTptr;
      if (v.empty())
	f=eval(a,1,contextptr);
      else {
	if (u==at_remove)
	  v.push_back(a);
	else {
	  if (!cmd && ckmatrix(m) && v.front().type==_IDNT){ // ex: m.reshape(2,4)
	    b=v.front();
	    v.front()=m;
	    v=makevecteur(b,gen(v,_SEQ__VECT));
	  }
	  else {
	    if (u==at_of && v.size()==2){
	      if (v.back().type==_VECT){
		vecteur w=*v.back()._VECTptr;
		w.insert(w.begin(),a);
		v=makevecteur(v.front(),gen(w,_SEQ__VECT));
	      }
	      else
		v=makevecteur(v.front(),makesequence(a,v.back()));
	    }
	    else
	      v.insert(v.begin(),a);
	  }
	}
	f=gen(v,f.type==_VECT?f.subtype:_SEQ__VECT);
      }
      f=symbolic(u,f);
    }
    f=eval(f,eval_level(contextptr),contextptr);
    if ((a.type==_IDNT || a.type==_SYMB) && (u==at_revlist || u==at_reverse || u==at_sort || u==at_append || u==at_prepend || u==at_concat || u==at_extend || u==at_rotate || u==at_shift || u==at_suppress || u==at_remove || u==at_insert ))
      return sto(f,a,contextptr);
    return f;
  }
  static const char _struct_dot_s []=".";
  static define_unary_function_eval4_index (175,__struct_dot,&_struct_dot,_struct_dot_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr5( at_struct_dot ,alias_at_struct_dot,&__struct_dot,_QUOTE_ARGUMENTS,true);

  gen _giac_assert(const gen & args,GIAC_CONTEXT){
    gen test=args;
    string msg(gettext("assert failure: ")+args.print(contextptr));
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      test=args._VECTptr->back();
      if (test.type==_STRNG) msg=*test._STRNGptr; else msg=test.print(contextptr);
      test=args._VECTptr->front();
    }
    test=equaltosame(test);
    int evallevel=eval_level(contextptr);
    test=equaltosame(test).eval(evallevel,contextptr);
    if (!is_integer(test))
      test=test.evalf_double(evallevel,contextptr);
    if (!is_integral(test) || test.val!=1)
      return gensizeerr(msg);
    return 1;
  }
  static const char _giac_assert_s []="assert";
  static define_unary_function_eval_quoted (__giac_assert,&_giac_assert,_giac_assert_s);
  define_unary_function_ptr5( at_giac_assert ,alias_at_giac_assert,&__giac_assert,_QUOTE_ARGUMENTS,T_RETURN);

  gen _index(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen l;
    if (args._VECTptr->front().type==_STRNG)
      l=_find(args,contextptr);
    else
      l=_find(makesequence(args._VECTptr->back(),args._VECTptr->front()),contextptr);
    if (l.type!=_VECT)
      return l;
    if (l._VECTptr->empty())
      return gensizeerr(contextptr);
    return l._VECTptr->front();
  }
  static const char _index_s []="index";
  static define_unary_function_eval (__index,&_index,_index_s);
  define_unary_function_ptr5( at_index ,alias_at_index,&__index,0,true);

  gen _giac_bool(const gen & args,GIAC_CONTEXT){
    bool b=args.type==_VECT?args._VECTptr->empty():is_exactly_zero(args);
    gen r=b?0:1;
    r.subtype=_INT_BOOLEAN;
    return r;
  }
  static const char _giac_bool_s []="bool";
  static define_unary_function_eval (__giac_bool,&_giac_bool,_giac_bool_s);
  define_unary_function_ptr5( at_giac_bool ,alias_at_giac_bool,&__giac_bool,0,true);


  gen _heapify(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();    
    gen f=at_inferieur_strict_sort;
    if (args.type==_SEQ__VECT && itend-it==2 && it->type==_VECT){
      f=*it;
      it=f._VECTptr->begin();
      itend=f._VECTptr->end();
      f=args._VECTptr->back();
    }
    make_heap(it,itend,gen_sort(f,contextptr));
    return 1;
  }
  static const char _heapify_s []="heapify";
  static define_unary_function_eval (__heapify,&_heapify,_heapify_s);
  define_unary_function_ptr5( at_heapify ,alias_at_heapify,&__heapify,0,true);

  gen _heappop(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    gen v=args;
    iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
    gen f=at_inferieur_strict_sort;
    if (args.type==_SEQ__VECT && itend-it==2 && it->type==_VECT){
      v=*it;
      it=v._VECTptr->begin();
      itend=v._VECTptr->end();
      f=args._VECTptr->back();
    }
    if (itend==it)
      return gendimerr(contextptr);
    pop_heap(it,itend,gen_sort(f,contextptr));
    v._VECTptr->pop_back();
    return *itend;
  }
  static const char _heappop_s []="heappop";
  static define_unary_function_eval (__heappop,&_heappop,_heappop_s);
  define_unary_function_ptr5( at_heappop ,alias_at_heappop,&__heappop,0,true);

  gen _heappush(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    gen f=at_inferieur_strict_sort;
    if (args.type!=_VECT || args._VECTptr->size()<2)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    if (v.size()==3) f=v[2];
    if (v[0].type!=_VECT) return gensizeerr(contextptr);
    v[0]._VECTptr->push_back(v[1]);
    iterateur it=v[0]._VECTptr->begin(),itend=v[0]._VECTptr->end();
    push_heap(it,itend,gen_sort(f,contextptr));
    return v[0];
  }
  static const char _heappush_s []="heappush";
  static define_unary_function_eval (__heappush,&_heappush,_heappush_s);
  define_unary_function_ptr5( at_heappush ,alias_at_heappush,&__heappush,0,true);


#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC

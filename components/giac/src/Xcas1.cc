// -*- mode:C++ ; compile-command: "g++-3.4 -I. -I.. -g -c Xcas1.cc -Wall -DHAVE_CONFIG_H -DIN_GIAC" -*-
/*
 *  Copyright (C) 2005,2014 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "first.h"
#include <string>
#ifdef HAVE_LIBFLTK
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Counter.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Multiline_Output.H>
#endif
#include "Xcas1.h"
#include "Input.h"
#include "Equation.h"
#include "Graph.h"
#include "Graph3d.h"
#include "Tableur.h"
#include "Editeur.h"
#include "Print.h"
#include "Help1.h"
#ifdef EMCC2
#include "hist.h"
#include <emscripten.h>
#endif
#include <iostream>
#include <fstream>
#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif
#include <typeinfo>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h> // auto-recovery function
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_LIBPTHREAD
#include <semaphore.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "global.h"
#include "misc.h"
#include "gen.h"
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
using namespace std;
using namespace giac;

#ifndef NO_NAMESPACE_XCAS
namespace xcas {
#endif // ndef NO_NAMESPACE_XCAS

#ifdef HAVE_LIBFLTK
  std::vector<Fl_Widget *> modal_w_tab;

  bool interrupt_button = true ;
  bool geo_run=false;
  bool sheet_run=false;
  bool recovery_mode=false;
  int fonts_available=1;

  void (*initialize_function)()=0;

  Xcas_config_type Xcas_config;
  void (* menu2rpn_callback)(Fl_Widget *,void *)=0;
  Enlargable_Multiline_Output *Xcas_help_output =0 ;
  xcas::Graph2d *Xcas_DispG=0;
  xcas::Equation * Xcas_PrintG=0;
  int show_xcas_dispg=0,redraw_turtle=0;
  std::string xcas_paused="";
  int xcas_dispg_entries=0;
  Fl_Window * Xcas_DispG_Window=0;
  Fl_Window * Xcas_Main_Window=0;
  Fl_Widget * Xcas_MainTab=0;
  Fl_Button *Xcas_DispG_Cancel=0;
  Fl_Button *Xcas_Cancel=0;
  bool file_save_context=true;

  void xcas_gprintf(unsigned special,const std::string & format,const vecteur & v,GIAC_CONTEXT){
    if (Xcas_PrintG){
      Fl::lock();
      gen g=Xcas_PrintG->get_data();
      Fl::unlock();
      vecteur w=makevecteur(string2gen("",false));//makevecteur(string2gen("",false),string2gen("Step by step console",false));
      if (g.type==_VECT) w=*g._VECTptr;
      // add format,v at the end of w
      int posnl=0;
      unsigned i=0;
      for (;posnl<int(format.size());){
	int nl=int(format.find('\n',posnl));
	string curs;
	bool finish = nl<0 || nl>=int(format.size());
	if (finish)
	  curs=format.substr(posnl,format.size()-posnl);
	else {
	  curs=format.substr(posnl,nl-posnl);
	  posnl=nl+1;
	}
	vecteur cur;
	for (;i<v.size() && !curs.empty();++i){
	  int p=int(curs.find("%gen"));
	  if (p<0 || p>=int(curs.size()))
	    break;
	  string cursp=curs.substr(0,p);
	  if (!cursp.empty())
	    cur.push_back(string2gen(cursp,false));
	  cur.push_back(v[i]);
	  curs=curs.substr(p+4,curs.size()-p-4);
	}
	if (!curs.empty())
	  cur.push_back(string2gen(curs,false));
	if (cur.empty())
	  continue;
	if (cur.size()==1)
	  w.push_back(cur.front());
	else
	  w.push_back(gen(cur,_SEQ__VECT));
	if (finish)
	  break;
      }
      g=gen(w,_HIST__VECT);
      Fl::lock();
      Xcas_PrintG->set_data(g);
      Fl::unlock();
    }
  }

  // debugger variables
  int xcas_debug_ok,xcas_current_instruction;
  Fl_Double_Window * Xcas_Debug_Window=0;
  Fl_Tile* Xcas_debug_tile =0;
  Fl_Browser* Xcas_source_browser =0;
  xcas::Multiline_Input_tab* Xcas_debug_input =0;
  Fl_Output* Xcas_debug_messages =0;
  Fl_Group* Xcas_debug_buttons =0;
  Fl_Button* Xcas_sst_button=0,*Xcas_sst_in_button=0,*Xcas_cont_button=0,* Xcas_kill_button =0, * Xcas_break_button=0,* Xcas_rmbrk_button=0,*Xcas_watch_button =0,*Xcas_rmwatch_button=0;
  Fl_Browser* Xcas_variable_browser=0;
  static void cb_Xcas_source_browser(Fl_Browser*, void* user) {
    std::string s("breakpoint(");
    giac::context * contextptr =(giac::context *) user; 
    s += giac::debug_ptr(contextptr)->debug_prog_name->print();
    s += ',';
    s += giac::print_INT_(Xcas_source_browser->value());
    s += ')';
    Xcas_debug_input->value(s.c_str());
    Xcas_debug_input->position(s.size()-1,s.size()-1);
    Fl::focus(Xcas_debug_input);
    Fl::flush();
    usleep(200000);
    Xcas_source_browser->value(xcas_current_instruction);
  }

  static void cb_Xcas_debug_input(xcas::Multiline_Input_tab*, void*) {
    xcas_debug_ok=true;
  }
  
  static void cb_Xcas_sst_button(Fl_Button*, void*) {
    Xcas_debug_input->value("sst");
    xcas_debug_ok=true;
  }
  
  static void cb_Xcas_sst_in_button(Fl_Button*, void*) {
    Xcas_debug_input->value("sst_in");
    xcas_debug_ok=true;
  }

  static void cb_Xcas_cont_button(Fl_Button*, void*) {
    Xcas_debug_input->value("cont");
    xcas_debug_ok=true;
  }
  
  static void cb_Xcas_kill_button(Fl_Button*, void*) {
    Xcas_debug_input->value("kill");
    Xcas_debug_input->position(4,4);
    Fl::focus(Xcas_debug_input);
  }

  static void cb_Xcas_break_button(Fl_Button*, void* user) {
    string s("breakpoint(");
    giac::context * contextptr =(giac::context *) user; 
    s += giac::debug_ptr(contextptr)->debug_prog_name->print();
    s += ',';
    s += giac::print_INT_(Xcas_source_browser->value());
    s += ')';
    Xcas_debug_input->value(s.c_str());
    Xcas_debug_input->position(s.size()-1,s.size()-1);
    Fl::focus(Xcas_debug_input);
  }
  
  static void cb_Xcas_rmbrk_button(Fl_Button*, void* user) {
    string s("rmbreakpoint(");
    giac::context * contextptr =(giac::context *) user; 
    s += giac::debug_ptr(contextptr)->debug_prog_name->print();
    s += ',';
    s += giac::print_INT_(Xcas_source_browser->value());
    s += ')';
    Xcas_debug_input->value(s.c_str());
    Xcas_debug_input->position(s.size()-1,s.size()-1);
    Fl::focus(Xcas_debug_input);
  }
  
  static void cb_Xcas_watch_button(Fl_Button*, void*) {
    Xcas_debug_input->value("watch(");
    Xcas_debug_input->position(6,6);
    Fl::focus(Xcas_debug_input);
  }

  static void cb_Xcas_rmwatch_button(Fl_Button*, void*) {
    Xcas_debug_input->value("rmwatch(");
    Xcas_debug_input->position(8,8);
    Fl::focus(Xcas_debug_input);
  }

  static void cb_Xcas_variable_browser(Fl_Browser*, void*) {
    Xcas_debug_input->insert(Xcas_variable_browser->text(Xcas_variable_browser->value()));
  }

  // end debugger callbacks and defs

  int autosave_time = 60 ; /* default save session every 60 seconds */
  bool autosave_disabled = false;

  bool find_fold_autosave_function(bool warn_user){
    History_Fold * hf=0;
    Fl_Widget * w = xcas::Xcas_input_focus;
    for (;w;){
      if ( (hf=dynamic_cast<History_Fold *>(w)) )
	break;
      w=w->parent();
    }
    if (hf)
      return hf->autosave(warn_user);
    else
      return false;
  }

  bool (*autosave_function)(bool) = find_fold_autosave_function ;
  void (*idle_function)()=0;

  unsigned max_debug_printsize=1000;
  void Xcas_debugguer(int status,giac::context * contextptr){
    if (debug_ptr(contextptr) && debug_ptr(contextptr)->debug_contextptr)
      contextptr = debug_ptr(contextptr)->debug_contextptr;
    // 2 -> debug, 3 ->wait click
    if (status==3){
      vecteur v(gen2vecteur(Xcas_DispG?Xcas_DispG->waiting_click_value:undef));
      if (v.empty()){
	if (Xcas_DispG_Window) Xcas_DispG_Window->show();
	if (Xcas_DispG_Cancel) Xcas_DispG_Cancel->show();
	if (Xcas_DispG) Xcas_DispG->waiting_click=true;
	for (;;) {
	  Fl::wait();
	  if (Xcas_DispG_Window && !Xcas_DispG_Window->visible()){
	    Xcas_DispG->waiting_click=false;
	    Xcas_DispG->waiting_click_value=undef;
	  } 
	  bool wait=Xcas_DispG?Xcas_DispG->waiting_click:false;
	  if (!wait)
	    break;
	}
	if (Xcas_DispG) Xcas_DispG->waiting_click=false;
	if (Xcas_DispG_Cancel) Xcas_DispG_Cancel->hide();
	thread_eval_status(1,contextptr);
	return;
      }
      v=inputform_pre_analysis(v,contextptr);
      gen res=makeform(v,contextptr);
      if (Xcas_DispG)
	Xcas_DispG->waiting_click_value=inputform_post_analysis(v,res,contextptr);
      thread_eval_status(1,contextptr);
      return;
    }
    if (status==2){
      if (!Xcas_Debug_Window){
	int dx,dy;
	if (xcas::Xcas_input_focus && xcas::Xcas_input_focus->window()){
	  dx=8*(2*xcas::Xcas_input_focus->window()->w()/24);
	  dy=11*(2*xcas::Xcas_input_focus->window()->h()/30);
	}
	else {
	  dx=400;
	  dy=500;
	}
	Fl_Double_Window* o = Xcas_Debug_Window = new Fl_Double_Window(dx,dy, gettext("Xcas Debug Window"));
	{ 
	  Fl_Tile* o = Xcas_debug_tile = new Fl_Tile(0, 0, dx,dy);
	  o->box(FL_FLAT_BOX);
	  { Fl_Browser* o = Xcas_source_browser = new Fl_Browser(0,0,dx,4*dy/11);
	  o->tooltip(gettext("Show the source of the program"));
	  o->type(2);
	  o->callback((Fl_Callback*)cb_Xcas_source_browser);
	  }
	  { xcas::Multiline_Input_tab* o = Xcas_debug_input = new xcas::Multiline_Input_tab(dx/8,4*dy/11, dx-dx/8,dy/11, gettext("eval"));
	  o->type(4);
	  o->box(FL_DOWN_BOX);
	  o->color(FL_BACKGROUND2_COLOR);
	  o->selection_color(FL_SELECTION_COLOR);
	  o->labeltype(FL_NORMAL_LABEL);
	  o->labelfont(0);
	  o->labelsize(14);
	  o->labelcolor(FL_FOREGROUND_COLOR);
	  o->textcolor(1);
	  o->callback((Fl_Callback*)cb_Xcas_debug_input);
	  o->align(FL_ALIGN_LEFT);
	  o->when(FL_WHEN_ENTER_KEY);
	  }
	  { Fl_Output* o = Xcas_debug_messages = new Fl_Output(0,5*dy/11,dx,2*dy/11);
	  o->type(12);
	  o->labelcolor(FL_FOREGROUND_COLOR);
	  o->textcolor(4);
	  }
	  { Fl_Group* o = Xcas_debug_buttons = new Fl_Group(0,7*dy/11,dx,dy/11);
	  { Fl_Button* o = Xcas_sst_button = new Fl_Button(0,7*dy/11,dx/8,dy/11, gettext("sst F5"));
	  o->tooltip(gettext("Execute current line, skip function"));
	  o->callback((Fl_Callback*)cb_Xcas_sst_button);
	  o->shortcut(0xffc2);
	  }
	  { Fl_Button* o = Xcas_sst_in_button = new Fl_Button(dx/8,7*dy/11, dx/8, dy/11, gettext("in F6"));
	  o->tooltip(gettext("Execute current line, step in function"));
	  o->callback((Fl_Callback*)cb_Xcas_sst_in_button);
	  o->shortcut(0xffc3);
	  }
	  { Fl_Button* o = Xcas_cont_button = new Fl_Button(2*dx/8,7*dy/11, dx/8, dy/11, gettext("cont F7"));
	  o->tooltip(gettext("Continue execution until next breakpoint"));
	  o->callback((Fl_Callback*)cb_Xcas_cont_button);
	  o->shortcut(0xffc4);
	  }
	  { Fl_Button* o = Xcas_kill_button = new Fl_Button(3*dx/8,7*dy/11, dx/8, dy/11, gettext("kill"));
	  o->tooltip(gettext("Kill current program"));
	  o->callback((Fl_Callback*)cb_Xcas_kill_button);
	  }
	  { Fl_Button* o = Xcas_break_button = new Fl_Button(4*dx/8,7*dy/11, dx/8, dy/11, gettext("break"));
	  o->tooltip(gettext("Add a breakpoint"));
	  o->callback((Fl_Callback*)cb_Xcas_break_button);
	  }
	  { Fl_Button* o = Xcas_rmbrk_button = new Fl_Button(5*dx/8,7*dy/11, dx/8, dy/11, gettext("rmbrk"));
	  o->tooltip(gettext("Remove a breakpoint"));
	  o->callback((Fl_Callback*)cb_Xcas_rmbrk_button);
	  }
	  { Fl_Button* o = Xcas_watch_button = new Fl_Button(6*dx/8,7*dy/11,dx/8, dy/11, gettext("watch"));
	  o->tooltip(gettext("Add a variable name to the watch"));
	  o->callback((Fl_Callback*)cb_Xcas_watch_button);
	  }
	  { Fl_Button* o = Xcas_rmwatch_button = new Fl_Button(7*dx/8,7*dy/11,dx/8, dy/11, gettext("rmwtch"));
	  o->tooltip(gettext("Add a variable name to the watch"));
	  o->callback((Fl_Callback*)cb_Xcas_rmwatch_button);
	  }
	  o->end();
	  }
	  { Fl_Browser* o = Xcas_variable_browser = new Fl_Browser(0,8*dy/11,dx,3*dy/11);
	  o->tooltip(gettext("Show watch variables"));
	  o->type(2);
	  o->callback((Fl_Callback*)cb_Xcas_variable_browser);
	  }
	  o->end();
	}
	o->end();
	o->resizable(o);	
      }
      Xcas_source_browser->user_data( (void *) contextptr);
      Xcas_break_button->user_data( (void *) contextptr);
      Xcas_rmbrk_button->user_data( (void *) contextptr);
      if (Xcas_Main_Window && Xcas_Debug_Window) 
	change_group_fontsize(Xcas_Debug_Window,Xcas_Main_Window->labelsize());
      if (Xcas_Debug_Window){
	Xcas_Debug_Window->show();
	// Xcas_Debug_Window->set_modal();
	Xcas_debug_input->when(FL_WHEN_ENTER_KEY|FL_WHEN_NOT_CHANGED);
      }
      // Debugging mode
      // cerr << "Debugging" << '\n';
      debug_struct * dbgptr=debug_ptr(contextptr);
      if (dbgptr){
	if (dbgptr->debug_info_ptr && dbgptr->debug_info_ptr->type==_VECT){
	  vecteur & w =*dbgptr->debug_info_ptr->_VECTptr;
	  // w[0]=function, args,
	  // w[1]=breakpoints
	  // w[2] = instruction evaled or program source
	  // w[3]= evaluation result
	  // w[4]= current instruction number 
	  // w[5] = watch vector, w[6] = watch values
	  string msg;
	  msg += "eval("+w[2].print(contextptr) + ")= " + w[3].print(contextptr) + '\n';
	  msg += "Stopped in ";
	  if ( w[0].type==_VECT && !w[0]._VECTptr->empty() ){
	    if (!dbgptr->debug_prog_name)
	      dbgptr->debug_prog_name=new gen;
	    if (*dbgptr->debug_prog_name!=w[0]._VECTptr->front()){
	      gen prog=w[0]._VECTptr->front();
	      *dbgptr->debug_prog_name=prog;
	      if (prog.type==_IDNT)
		prog=prog.eval(1,contextptr);
	      if (prog.is_symb_of_sommet(at_program) && prog.type==_VECT && prog._VECTptr->size()==3 )
		prog=prog._VECTptr->back();
	      if (prog.type==_SYMB)
		prog=prog._SYMBptr->feuille;
	      if (prog.type==_VECT && prog._VECTptr->size()==3)
		prog=prog._VECTptr->back();
	      w[2]=prog;
	      Xcas_source_browser->clear();
	      vector<string> vs;
	      if (w[2].type==_VECT)
		debug_print(*w[2]._VECTptr,vs,contextptr);
	      else
		debug_print(w[2],vs,contextptr);
	      vector<string>::iterator it=vs.begin(),itend=vs.end();
	      for (;it!=itend;++it){
		string & cur=*it; // replace \n by space
		for (;;){
		  size_t pos=cur.find('\n');
		  if (pos<0 || pos>=cur.size())
		    break;
		  cur[pos]=' ';
		}
		Xcas_source_browser->add(cur.c_str());
	      }
	    }
	    msg += dbgptr->debug_prog_name->print(contextptr) + "(" + gen(vecteur(w[0]._VECTptr->begin()+1,w[0]._VECTptr->end()),_SEQ__VECT).print(contextptr)+")";
	    msg += ",\nbreakpoint "+w[1].print(contextptr);
	  } // if (w[0].type==_VECT)
	  else
	    msg += w[0].print(contextptr);
	  msg += ",\nline " + w[4].print(contextptr);
	  Xcas_debug_messages->value(msg.c_str());
	  gen w_in(w[5]),w_out(w[6]);
	  if ( (w_in.type==_VECT) && (w_out.type==_VECT) ){
	    int pos=max(Xcas_variable_browser->value(),1);
	    Xcas_variable_browser->clear();
	    const_iterateur it=w_in._VECTptr->begin(),itend=w_in._VECTptr->end();
	    const_iterateur jt=w_out._VECTptr->begin(),jtend=w_out._VECTptr->end();
	    for (;(it!=itend)&&(jt!=jtend);++it,++jt){
	      string tmps = replace(jt->print(contextptr),'\n',' ');
	      if (tmps.size()>max_debug_printsize)
		tmps=tmps.substr(0,max_debug_printsize);
	      Xcas_variable_browser->add( (it->print(contextptr) + " := " + tmps ).c_str());
	    }
	    Xcas_variable_browser->value(giacmin(pos,w_in._VECTptr->size()));
	  }
	  xcas_current_instruction=w[4].val;
	  Xcas_source_browser->value(w[4].val);
	  /* console mode debugging
	     if (dbgptr->debug_refresh){
	     if (dbgptr->fast_debug_info_ptr)
	     cerr << *dbgptr->fast_debug_info_ptr << '\n';
	     }
	     else {
	     if (dbgptr->debug_info_ptr)
	     cerr << *dbgptr->debug_info_ptr << '\n';
	       }
	  */
	}
      }
      gen g(at_sst);
      gen nxt;
      xcas_debug_ok=false;
      while (Xcas_Debug_Window->shown() && !xcas_debug_ok){
	Fl::wait();
	int cs=context_list().size(),ci=0,status=0;
	for (;ci<cs;++ci){
	  status=check_thread(context_list()[ci]);
	}
      }
      /* console mode debugging
	 char buf[10000];
	 cin.getline(buf,10000-1,'\n');
	 if (buf[0]){
	 try {
	 g=gen(buf);
	 }
	 catch (std::runtime_error & err){
	 cerr << err.what();
	 }
	 }
      */
      if (!Xcas_Debug_Window->visible())
	g=at_kill;
      else {
	string buf(Xcas_debug_input->value());
	if (!buf.empty()){
	  try {
	    g=gen(buf,contextptr);
	  }
	  catch (std::runtime_error & err){
	    cerr << err.what();
	  }
	}
      }
      *dbgptr->fast_debug_info_ptr = g;
      thread_eval_status(1,contextptr);
    }
  }

  void Xcas_interrupt_cb(void){
    static int counter=0;
    ++counter;
    if (counter & 0xf)
      return;
    Fl::check();
  }

  void Xcas_idle_function(void * dontcheck){
    static int initialized=-1;
    static int last_save;
    struct timeval cur;
    struct timezone tz;
    if (initialized==-1){
      gettimeofday(&cur,&tz);
      last_save=cur.tv_sec;
    }
    int cs=context_list().size(),ci=0,status=0;
    context * cptr=0;
    for (;ci<cs;++ci){
      cptr=context_list()[ci];
      if (!dontcheck || (void *)cptr!=dontcheck)
	status=check_thread(cptr);
      if (status>1)
	break;
    }
    if (ci<cs){
      context * contextptr=context_list()[ci];
      Xcas_debugguer(status,contextptr);
    }
    if (Xcas_Debug_Window) {
      if (status<2 && Xcas_Debug_Window->shown()){
	if (cptr){
	  usleep(100000);
	  status=check_thread(cptr);
	}
	if (status<2)
	  Xcas_Debug_Window->hide();
	else
	  return; // moved from below otherwise after debugging STOP button is always on
      }
    }
    bool lock=true; // added because DispG freeze in linux
#ifndef EMCC2
    if (Xcas_DispG_Window){
      if (show_xcas_dispg){
	lock=false;
	if (show_xcas_dispg & 2){
	  if (!Xcas_DispG_Window->visible())
	    Xcas_DispG->autoscale();
	  if (show_xcas_dispg & 1){
	    Xcas_DispG_Window->show();
	    Xcas_DispG_Window->iconize();
	  }
	  else
	    Xcas_DispG_Window->show();
	}
	else
	  Xcas_DispG_Window->hide();
	show_xcas_dispg=0;
      }
    }
#endif
    if (xcas_paused!=""){
      Xcas_Main_Window->redraw();
      if (Xcas_DispG) Xcas_DispG->redraw();
      fl_message("%s",xcas_paused.c_str());
      xcas_paused="";
    }
    ++initialized;
    if (initialized % 5){
      if (lock) Fl::unlock();
#ifdef WIN32
      usleep(10000);
#else
      usleep(1000);
#endif
      if (lock) Fl::lock();
      return;
    }
    if (!initialized && initialize_function)
      initialize_function();
    if (
#ifdef WIN32
	!(initialized%20) && 
#endif
	idle_function
	)
      idle_function();
    /* autosave */
    gettimeofday(&cur,&tz);
    if (autosave_time>0 && !autosave_disabled && autosave_function && double(cur.tv_sec-last_save)>autosave_time){
      if (autosave_function(false))
	last_save=cur.tv_sec;
      else
	last_save=cur.tv_sec-autosave_time/2;
    }
    vector<Graph2d3d *>::const_iterator it=animations.begin(),itend=animations.end();
    for (;it!=itend;++it){
      Graph2d3d * gr = *it;
      if (!gr->paused && gr->animation_dt>0){
        double dt=cur.tv_sec-gr->animation_last.tv_sec+double(cur.tv_usec-gr->animation_last.tv_usec)/1e6;
	if (dt>gr->animation_dt){
	  gr->redraw();
	}
      }
    }
  }

  bool operator < (const time_string & ts1,const time_string & ts2){
    if (ts1.t!=ts2.t)
      return ts1.t < ts2.t;
    else
      return ts1.s < ts2.s;
  }
  
  // Check for auto-recovery data in directory s
  bool has_autorecover_data(const string & s_orig,vector<time_string> & newest){
    DIR *dp;
    struct dirent *ep;
    string s=s_orig;
    if (!s.empty() && s[s.size()-1]!='/')
      s += '/';
    else
      s += '/';
    dp = opendir (s.c_str());
    if (dp != NULL){
      while ( (ep = readdir (dp)) ){
	string cur(ep->d_name);
	if (cur.size()>13&& cur.substr(0,10)=="xcas_auto_"){ 
	  string curext=cur.substr(cur.size()-3,3);
	  if (curext=="xws"){
	    struct stat st;
	    if (!stat((s+ep->d_name).c_str(),&st)){
	      time_t & t=st.st_mtime; // last modif
	      newest.push_back(time_string(t,s+cur));
	    }
	  }
	}
      }
      (void) closedir (dp);
      if (newest.size()==0) // no file
	return false;
      return true;
    }
    else
      return false; // Couldn't open the directory
  }

  bool stream_copy(std::istream & in,std::ostream & out) {
    char c;
    while (in){
      if (!out){
	return false;
      }
      c=in.get();
      if (in.eof()){
	return true;
      }
      out << c ;
    }
    return false;
  }
  

  bool add_user_menu(Fl_Menu_ *m,const string & s, const string & doc_prefix,Fl_Callback * cb){
    string menufile;
    bool menufile_found=false;
    if (is_file_available(s.c_str())){
      menufile_found=true;
      menufile=s;
    }
    else {
      if (getenv("XCAS_HELP")){
	menufile=getenv("XCAS_HELP");
	int ms=menufile.size();
	for (--ms;ms>0;--ms){
	  if (menufile[ms]=='/')
	    break;
	}
	if (ms)
	  menufile=menufile.substr(0,ms+1)+doc_prefix+s;
      }
      else
	menufile=giac::giac_aide_dir()+doc_prefix+s;
      menufile_found=is_file_available(menufile.c_str());
    }
    if (!menufile_found){
      cerr << "// Unable to open menu file "<< menufile << '\n';
      return false;
    }
    cerr << "// Using menu file " << menufile << '\n';
    // Now reading commandnames from file for menus
    // Syntax is menu/submenu/item, callback inserts item name
    ifstream in(menufile.c_str());
    string lu;
    int BUFSIZE=10000,endpos;
    char buf[BUFSIZE+1];
    while (in){
      in.getline(buf,BUFSIZE,'\n');
      endpos=strlen(buf);
      if (endpos){
	if (buf[endpos-1]=='\n')
	  buf[endpos-1]=0;
	lu=buf;
      }
      if (in.eof())
	break;
      m->add(lu.c_str(),0,cb);
    }
    return true;  
  }

  void nextfl_menu(Fl_Menu_Item * & m){
    if (m->submenu()){
      ++m;
      for (;m->text;)
	nextfl_menu(m);
    }
    ++m;
  }

  void copy_menu(Fl_Menu_ * menu,const string & prefix,Fl_Menu_Item * & m){
    for (;m->text;++m){
      if (m->submenu()){
	string prefix2=prefix+m->label();
	prefix2 += '/';
	++m;
	copy_menu(menu,prefix2,m);
      }
      else 
	menu->add((prefix+m->text).c_str(),m->shortcut_,m->callback_,m->user_data_,m->flags);
    }
  }

  vecteur fl_menu2rpn_menu(Fl_Menu_Item * & m){
    vecteur res;
    ++m;
    for (;m->text;++m){
      if (m->submenu()){
	string s=m->label();
	vecteur tmp(fl_menu2rpn_menu(m));
	if (!tmp.empty())
	  res.push_back(makevecteur(string2gen(s,false),tmp));
      }
      else {
	gen g;
	if (m->callback_==menu2rpn_callback){
	  string s(m->label()); int i=0;
          if (s.empty() || s[0]=='-')
            continue;
	  for (;i<s.size();++i){
	    if (s[i]==':'){
	      s=s.substr(0,i);
	      break;
	    }
	  }
	  find_or_make_symbol(s.c_str(),g,0,false,context0);
	  // g=gen(string("'")+m->label()+string("'"));
	  if (g.is_symb_of_sommet(at_quote))
	    g=g._SYMBptr->feuille; 
	  res.push_back(g);
	}
      }
    }
    return res;
  }

  void change_menu_fontsize(Fl_Menu * & m,int labelfontsize){
    if (!m->text)
      return;
    m->labelsize(labelfontsize);
    if (m->submenu()){
      ++m;
      for (;m->text;++m)
	change_menu_fontsize(m,labelfontsize);
    }
  }

  void change_menu_fontsize(Fl_Menu_Item * m,int n,int labelfontsize){
    for (int i=0;i<n;++i){
      change_menu_fontsize(m,labelfontsize);
      ++m;
    }
  }

  void change_equation_fontsize(Equation * eq,int labelfontsize){
    if (eq->attr.fontsize==labelfontsize)
      return;
    eq->attr.fontsize=labelfontsize;
    eq->labelsize(labelfontsize);
    eq->menubar->labelsize(labelfontsize);
    eq->resize(eq->x(),eq->y(),eq->w(),eq->h());
    eq->set_data(eq->get_data());
    eq->adjust_widget_size();
  }

  void change_group_fontsize(Fl_Widget * w,int labelfontsize){
    w->labelsize(labelfontsize);
    if (Fl_Scroll * scr=dynamic_cast<Fl_Scroll *>(w))
      scr->scrollbar_size((is_mobile()?3:2)*labelfontsize/2);
    w->redraw();
    if (Equation * eq=dynamic_cast<Equation *>(w)){
      change_equation_fontsize(eq,labelfontsize);
      //return;
    }
    Fl_Group * g = dynamic_cast<Fl_Group *>(w);
    if (!g)
      return;
    Fl_Font police=g->labelfont();
    Fl_Widget * o;
    Fl_Widget * const * ptr= g->array();
    int n=g->children();
    for (int i=0;i<n;++ptr,++i){
      o = *ptr;
      if (o->labelfont()!=FL_SYMBOL)
	o->labelfont(police);
      if (Fl_Menu * m=dynamic_cast<Fl_Menu *>(o)){
	change_menu_fontsize(m,labelfontsize);
	continue;
      }
      if (Fl_Menu_ * mb=dynamic_cast<Fl_Menu_ *>(o)){
	Fl_Menu_Item * mm=(Fl_Menu_Item *)mb->menu();
	for (;mm->text;++mm)
	  change_menu_fontsize(mm,labelfontsize);
      }
      if (Fl_Input_ * in=dynamic_cast<Fl_Input_ * >(o)){
	in->textsize(labelfontsize);
	in->textfont(police);
      }
      if (Equation * eq=dynamic_cast<Equation * >(o)){
	eq->labelfont(police);
	change_equation_fontsize(eq,labelfontsize);
      }
      if (Xcas_Text_Editor * xed=dynamic_cast<Xcas_Text_Editor *>(o)){
	xed->Fl_Text_Display::textsize(labelfontsize);
	xed->Fl_Text_Display::textfont(labelfontsize);
	vector<Fl_Text_Display::Style_Table_Entry> & v=xed->styletable;
	for (unsigned i=0;i<v.size();++i)
	  v[i].size=labelfontsize;
	xed->labelsize(labelfontsize);
	Editeur * ed =dynamic_cast<Editeur *>(xed->parent());
	if (!ed && !xed->tableur)
	  xed->resize_nl_before(1);
	xed->redraw();
      }
      if (Fl_Value_Input * v=dynamic_cast<Fl_Value_Input *>(o))
	v->textsize(labelfontsize);
      if (Fl_Browser * b=dynamic_cast<Fl_Browser *>(o))
	b->textsize(labelfontsize);
      if (Fl_Counter * cc=dynamic_cast<Fl_Counter *>(o))
	cc->textsize(labelfontsize);
      if (Fl_Group * og=dynamic_cast<Fl_Group *>(o))
	change_group_fontsize(og,labelfontsize);
      if (Fl_Browser_ * os =dynamic_cast<Fl_Browser_ *>(o))
	os->scrollbar_size(3*labelfontsize/2);
      if (HScroll * os =dynamic_cast<HScroll *>(o)){
	// resize childs according to new scrollbar width
	os->resize(os->x(),os->y(),os->w(),os->h());
      }
      if (Flv_Table_Gen * fl = dynamic_cast<Flv_Table_Gen *>(o)){
        int l=labelfontsize;
        if (fl->h()<400) l-=2;
	fl->global_style.font_size(l);
	fl->global_style.height(l+4);
	fl->redraw();
      }
      o->labelsize(labelfontsize);
      if (Graph2d3d * gr=dynamic_cast<Graph2d3d *>(o))
	if (gr->parent() && !dynamic_cast<Figure *>(gr->parent()) && !dynamic_cast<Tableur_Group *>(gr->parent()))
	  gr->resize_mouse_param_group(gr->legende_size);
    }
  }

  // Cut a string for display in a multiline area of size w using fontsize
  string cut_help(const string & s,int fontsize,int w){
    if (s.empty())
      return s;
    int sw;
    // find 1st cut and add cut_help of the rest of the string to it
    int begin=0,taille=s.size(),end; // cut is between begin and first \n
    for (end=0;end<taille;++end){
      if (s[end]=='\n')
	break;
    }
    fl_font(FL_HELVETICA,fontsize);
    sw=int(fl_width(s.substr(0,end).c_str()));
    if (sw<w){
      if (end==taille)
	return s;
      return s.substr(0,end+1)+cut_help(s.substr(end+1,taille-end-1),fontsize,w);
    }
    for (;end-begin>2;){
      int pos=(begin+end)/2,i; // find 1st space after middlepoint
      for (i=pos;i>begin;--i){
	if (s[i]==' ')
	  break;
      }
      if (i==begin){ // no space before, try after
	for (i=pos;i<end;++i){
	  if (s[i]==' ')
	    break;
	}
      }
      if (i==end){ 
	// No space at all, return 0 -> begin or 0 -> end if begin==0
	if (begin)
	  pos=begin;
	else {
	  if (end)
	    pos=end;
	  else
	    return s;
	}
	if (pos==taille)
	  return s;
	return s.substr(0,pos)+'\n'+cut_help(s.substr(pos,taille-pos),fontsize,w);
      }
      sw=int(fl_width(s.substr(0,i).c_str()));
      if (sw<w)
	begin=i;
      else
	end=i;
    }
    return s.substr(0,end+1)+cut_help(s.substr(end+1,taille-end-1),fontsize,w);
  }

  void help_output(const std::string & s,int language){
    giac::aide cur_aide=helpon(s,(*giac::vector_aide_ptr()),language,(*giac::vector_aide_ptr()).size(),false);
    if (Xcas_help_output){
      string result=cur_aide.cmd_name;
      if (!cur_aide.syntax.empty())
	result=result+"("+cur_aide.syntax+")\n";
      vector<localized_string>::const_iterator it=cur_aide.blabla.begin(),itend=cur_aide.blabla.end();
      for (;it!=itend;++it){
	if (it->language==language){
	  result = it->chaine +'\n'+result ;
	  break;
	}
      }
      std::vector<std::string>::const_iterator jt=cur_aide.examples.begin(),jtend=cur_aide.examples.end();
      for (;jt!=jtend;++jt)
	result = result + *jt + '\n';
      std::vector<indexed_string>::const_iterator kt=cur_aide.related.begin(),ktend=cur_aide.related.end();
      for (;kt!=ktend;++kt)
	result = result + "-> "+print_INT_(kt->index)+": "+kt->chaine+'\n';
      Xcas_help_output->value(result.c_str());
      Xcas_help_output->redraw();
    }
  }

  void browser_help(const giac::gen & g,int language){
    giac::gen f(g);
    string s;
    if (f.type==giac::_SYMB)
      f=f._SYMBptr->sommet;
    if (f.type==giac::_FUNC)
      s=f._FUNCptr->ptr()->s;
    giac::html_vtt=giac::html_help(giac::html_mtt,s);
    help_output(s,language);
    if (!giac::html_vtt.empty()){
      if (use_external_browser)
	giac::system_browser_command(giac::html_vtt.front());
      else {
	if (xcas::Xcas_help_window){
	  xcas::Xcas_help_window->load(giac::html_vtt.front().c_str());
	  if (!xcas::Xcas_help_window->visible())
	    xcas::Xcas_help_window->show();
	}
      }
    }
  }

  void check_browser_help(const giac::gen & g,int language){
    if (g.is_symb_of_sommet(giac::at_findhelp))
      browser_help(g._SYMBptr->feuille,language);
  }

  void dbgprint(const gen & g,vector<string> & vs){
    vecteur v(gen2vecteur(g));
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      vs.push_back(it->print(context0));
  }

  void fl_wait_0001(context * contextptr){
    static int t_sec=0,t_usec=0;
    int status=thread_eval_status(contextptr);
    Xcas_debugguer(status,contextptr);
    // don't flush too often
    timeval tp;
    if (!gettimeofday(&tp,0)){
      if (tp.tv_sec==t_sec && tp.tv_usec < t_usec+50000){
	usleep(100);
	return;
      }
      t_sec=tp.tv_sec;
      t_usec=tp.tv_usec ;
    }
#ifdef __APPLE__
    Fl::wait(0.001);
#else
    Fl::wait(0.0001);
#endif
    Xcas_idle_function(0);
  }

  giac::gen thread_eval(const giac::gen & g,int level,context * contextptr){
    // Remove idle function for wait to work
    // cerr << "remove idle " << '\n';
    Fl::remove_idle(xcas::Xcas_idle_function,0);
    gen res=giac::thread_eval(g,level,contextptr,fl_wait_0001);
    if (Xcas_Debug_Window) Xcas_Debug_Window->hide();
    fl_wait_0001(contextptr);
    // Re-add idle function
    Fl::add_idle(xcas::Xcas_idle_function,0);
    return res;
  }

  Fl_Widget * make_gen_output(Fl_Widget * w,const gen & evaled_g){
    Gen_Output * o = new Gen_Output(w->x(),w->y(),w->w(),w->labelsize()+7);
    o->textsize(w->labelsize());
    o->value(evaled_g);
    o->textcolor(FL_BLUE);
    fl_font(FL_HELVETICA,w->labelsize());
    int hss=int(fl_width(o->Fl_Output::value()))+4;
    if (hss<=w->w())
      return o;
    if (find_graph2d3d(w) || (evaled_g.type==_VECT && evaled_g.subtype==_LOGO__VECT))
      return o;
    o->resize(w->x(),w->y(),hss,w->labelsize()+7);
    Fl_Scroll * s = new Fl_Scroll(w->x(),w->y(),w->w(),2*w->labelsize()+9);
    s->hscrollbar.resize(s->hscrollbar.x(),w->y()+w->labelsize()+7,s->hscrollbar.w(),w->labelsize());
    s->box(FL_FLAT_BOX);
    s->end();
    s->add(o);
    return s;
  }    

  Fl_Widget * in_Xcas_eval(Fl_Widget * w,const giac::gen & evaled_g0,int pretty_output,GIAC_CONTEXT){
    gen evaled_g=evaled_g0;
    if (evaled_g.type==_MAP && evaled_g.subtype==1)
      evaled_g=giac::maptoarray(*evaled_g._MAPptr,contextptr);
    // cleanup
    giac::clear_prog_status(contextptr);
    giac::cleanup_context(contextptr);
    History_Pack * hp = get_history_pack(w);
    if (hp) Fl_Group::current(hp);
    if (calc_mode(contextptr)==-38)
      calc_mode(contextptr)=38;
    /*    FIXME: use simplifier only if the user ask for it
	  if (!g.is_symb_of_sommet(at_ifactor))
	  evaled_g=giac::_simplifier(evaled_g,contextptr); */
    if (pretty_output){
      int anim=giac::animations(evaled_g);
      if (evaled_g.is_symb_of_sommet(at_parameter) && evaled_g._SYMBptr->feuille.type==_VECT){
	Gen_Value_Slider * res = parameter2slider(evaled_g,contextptr);
	if (res){
	  res->resize(w->x()+3*w->labelsize(),w->y(),w->w()-3*w->labelsize(),w->labelsize());
	  res->labelsize(w->labelsize());
	  return res;
	}
      }
      int tt;
      if (evaled_g.type == _VECT && (tt=graph_output_type(evaled_g)) ){
	if (tt==4){
	  // Fl_Tile * g = new Fl_Tile(w->x(),w->y(),w->w(),max(220,w->w()/3));
	  // g->labelsize(w->labelsize());
	  Turtle * tu = new Turtle(w->x(),w->y(),w->w(),max(220,w->w()/3));
	  tu->turtleptr=&turtle_stack(contextptr);
	  //g->end();
	  //change_group_fontsize(g,w->labelsize());
	  //return g;
	  return tu;
	}
	Fl_Tile * g = new Fl_Tile(w->x(),w->y(),w->w(),max(220,w->w()/3));
	g->labelsize(w->labelsize());
	Graph2d3d * tmp;
#if 1 // def HAVE_LIBFLTK_GL
	if (is3d(evaled_g._VECTptr->back())){
#ifdef GRAPH_WINDOW
	  Fl_Window * win=new Fl_Window(w->x(),w->y(),w->w(),g->h());
	  tmp =new Graph3d(0,0,w->w(),g->h(),"",hp);
	  win->end(); win->show();
	  Fl_Group::current(g);
#else
	  tmp =new Graph3d(w->x(),w->y(),w->w(),g->h(),"",hp);
#endif
	  tmp->show();
	}
	else
#endif 
	  {
	    tmp=new Graph2d(w->x(),w->y(),w->w(),g->h(),"",hp);
	    if (Xcas_config.ortho)
	      tmp->orthonormalize();
	  }
	tmp->add(*evaled_g._VECTptr);
	tmp->init_tracemode(); tmp->tracemode_set();
	if (anim)
	  tmp->animation_dt=1./5;
	if (Xcas_config.autoscale)
	  tmp->autoscale();
	tmp->update_infos(evaled_g,contextptr);
	g->end();
	change_group_fontsize(g,w->labelsize());
	return g;
      }
      if (is_pnt_or_pixon(evaled_g) || anim){
	Fl_Tile * g = new Fl_Tile(w->x(),w->y(),w->w(),max(220,w->w()/3));
	g->labelsize(w->labelsize());
	Graph2d3d * res;
#if 1 // def HAVE_LIBFLTK_GL
	if (is3d(evaled_g)){
#ifdef GRAPH_WINDOW
	  Fl_Window * win=new Fl_Window(w->x(),w->y(),w->w(),g->h());
	  res = new Graph3d(0,0,w->w(),g->h(),"",hp);
	  win->end(); win->show();
	  g->add(win);
	  Fl_Group::current(g);
#else
	  res = new Graph3d(w->x(),w->y(),w->w(),g->h(),"",hp);
#endif
	  res->show();
	}
	else
#endif 
	  {
	    res=new Graph2d(w->x(),w->y(),w->w(),g->h(),"",hp);
	    if (Xcas_config.ortho)
	      res->orthonormalize();
	  }
	res->add(evaled_g); 
	res->init_tracemode(); res->tracemode_set();
	if (anim)
	  res->animation_dt=1./5;
	if (Xcas_config.autoscale)
	  res->autoscale();
	res->update_infos(evaled_g,contextptr);
	g->end();
	change_group_fontsize(g,w->labelsize());
	return g;
      }
      /*
      if (evaled_g.type==_DOUBLE_ || evaled_g.type==_INT_){
	Gen_Output * o = new Gen_Output(w->x(),w->y(),w->w(),w->labelsize()+7);
	o->textsize(w->labelsize());
	o->value(evaled_g);
	o->textcolor(FL_BLACK);
	fl_font(FL_HELVETICA,w->labelsize());
	int hss=int(fl_width(o->Fl_Output::value()))+4;
	if (hss<=w->w())
	  return o;
	else
	  delete o;
      }
      */
      unsigned ta=taille(evaled_g,max_prettyprint_equation);
      if (ta<max_prettyprint_equation){      
	giac::attributs attr(w->labelsize(),Xcas_equation_background_color,Xcas_equation_color);
	giac::gen varg=Equation_compute_size(evaled_g,attr,w->w(),contextptr);
	giac::eqwdata vv(Equation_total_size(varg));
	int maxh=w->window()?(3*w->window()->h())/5:1000;
	int scrollsize=3;
	if (vv.dx>w->w()-2*w->labelsize())
	  scrollsize=w->labelsize()+3;
	int h=min(vv.dy+scrollsize+3,maxh); // =max(min(vv.dy+20,400),60);
	Equation * res = new Equation(w->x(),w->y(),w->w(),h,"",evaled_g,attr,contextptr);
	res->box(FL_FLAT_BOX);
	return res;
      }
    }
    return make_gen_output(w,evaled_g);
  }

  void Xcas_eval_callback(const giac::gen & evaled_g,void * param){
    Fl_Widget * wid=static_cast<Fl_Widget *>(param);
    if (!wid)
      return;
    int hp_pos;
    History_Pack * hp = get_history_pack(wid,hp_pos);
    context * contextptr = hp?hp->contextptr:0;
    Fl_Group * gr = wid->parent();
    if (!hp || !gr)
      return;
    // CERR << "" << evaled_g << '\n';
#ifdef HAVE_LIBPTHREAD
    // cerr << "eval lock" << '\n';
    int locked=pthread_mutex_trylock(&interactive_mutex);
    if (locked){
      usleep(100); // was usleep(1000)
      locked=pthread_mutex_trylock(&interactive_mutex);
      if (locked){
	//cerr << "locked " << evaled_g << '\n' ;
	return ;
      }
    }
#endif
    bool b=io_graph(contextptr);
    io_graph(contextptr)=false;
    bool block=block_signal;
    block_signal=true;
    // int m=gr->children();
    // reset output
#ifdef WITH_MYOSTREAM
    logptr(&my_cerr,contextptr);
#else
    logptr(&std::cerr,contextptr);
#endif
    if (!giac::history_out(contextptr).empty() && giac::history_out(contextptr).size()==giac::history_in(contextptr).size())
      giac::history_out(contextptr).back()=evaled_g;
    else
      giac::history_out(contextptr).push_back(evaled_g);
    int pos=gr->find(wid);
    Fl_Widget * res =0;
    if (evaled_g.type==_STRNG && *evaled_g._STRNGptr=="Logo_turtle"){
      giac::context * ptr=(giac::context *) caseval("caseval contextptr");
      giac::gen g0=_avance(0,ptr);
      res=in_Xcas_eval(wid,g0,hp->pretty_output,ptr);
    }
    else
      res = in_Xcas_eval(wid,evaled_g,hp->pretty_output,contextptr);
    if (Log_Output * lout=find_log_output(gr))
      output_resize_parent(lout,false);
    if (res){
      // resize group/pack here
      gr->Fl_Widget::resize(gr->x(),gr->y(),gr->w(),gr->h()+res->h()-wid->h());
      res->resize(wid->x(),wid->y(),res->w(),res->h());
      int dy=res->h()-wid->h();
      gr->remove(wid);
      Graph2d3d * widgraph = 0;
      if (Fl_Group * widgr = dynamic_cast<Fl_Group *>(wid)){
	if (widgr->children())
	  widgraph=dynamic_cast<Graph2d3d *>(widgr->child(0));
      }
      Graph2d3d * resgraph = 0; 
      if (Fl_Group * widgr = dynamic_cast<Fl_Group *>(res)){
	if (widgr->children())
	  resgraph=dynamic_cast<Graph2d3d *>(widgr->child(0));
      }
      gr->insert(*res,pos);
      change_group_fontsize(gr,gr->labelsize());
      if (widgraph && resgraph){
	resgraph->copy(*widgraph);
	resgraph->resize(resgraph->x(),resgraph->y(),widgraph->w(),resgraph->h());
	resgraph->resize_mouse_param_group(gr->w()-widgraph->w());
      }
      // gr->resizable(gr);
      hp->resize();
      if (Fl_Scroll * s = dynamic_cast<Fl_Scroll *>(hp->parent())){
	int spos=s->yposition();
	if (spos+s->h()>hp->h()){
#ifdef _HAVE_FL_UTF8_HDR_
	  s->scroll_to(0,max(min(hp->h()-s->h(),spos+dy),0));
#else
	  s->position(0,max(min(hp->h()-s->h(),spos+dy),0));
#endif
	}
	s->redraw();
      }
      Fl_Group * group = parent_skip_scroll(hp);
      if (Logo * logo=dynamic_cast<Logo *>(group)){
	logo->redraw();
      }
#ifndef EMCC2
      // show DispG?
      if (hp->pretty_output && Xcas_DispG_Window && !Xcas_DispG_Window->visible()){
	int entries=Xcas_DispG->plot_instructions.size();
	if (entries>xcas_dispg_entries){
	  if (resgraph){
	    vecteur dispgv=vecteur(Xcas_DispG->plot_instructions.begin()+xcas_dispg_entries,Xcas_DispG->plot_instructions.end()),v1,v2;
	    aplatir(dispgv,v1);
	    aplatir(resgraph->plot_instructions,v2);
	    if (v1!=v2){
	      if (Xcas_DispG_Cancel) Xcas_DispG_Cancel->hide();
	      show_xcas_dispg=3;
	    }
	  }
	  else {
	    if (Xcas_DispG_Cancel) Xcas_DispG_Cancel->hide();
	    show_xcas_dispg=3;
	  }
	}
      }
#endif
      block_signal=block;
      io_graph(contextptr)=b;
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&interactive_mutex);
#endif
      // if hp has eval_below, call callback on next widget
      if (hp->eval_below){
	hp->next(hp_pos);
	/*
	Fl_Group * hpp = parent_skip_scroll(hp);
	if (hpp){
	  int N=hpp->children();
	  for (int i=0;i<N;++i){
	    Graph2d3d * geo = dynamic_cast<Graph2d3d *>(hpp->child(i));
	    if (geo){
	      geo->handle(FL_FOCUS);
	    }
	  }
	}
	*/
      }
      else {
	Fl_Group * hpp = parent_skip_scroll(hp);
	if (hpp){
	  int N=hpp->children();
	  for (int i=0;i<N;++i){
	    Graph2d3d * geo = dynamic_cast<Graph2d3d *>(hpp->child(i));
	    if (geo){
	      geo->add(evaled_g);
	      geo->no_handle=false;
	    }
	  }
	}
	if (resgraph) 
	  Fl::focus(resgraph);
	else
	  hp->focus(hp_pos+1,false);
      }
    } // if (res)
    else { // res==0 no output
      if (Fl_Input_ * i=dynamic_cast<Fl_Input_ * >(wid))
	i->value("No_output");
      hp->resize();
      block_signal=block;
      io_graph(contextptr)=b;
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&interactive_mutex);
#endif
    }
  }

  Fl_Widget * Xcas_eval(Fl_Widget * w,const giac::gen & g_){
    if (!w)
      return 0;
    if (debug_infolevel>=5)
      cerr << "eval " << g_ << '\n';
    Fl_Group * gr=w->parent();
    Fl_Group::current(gr);
    // Find history_pack above for context from widget 
    History_Pack * hp = get_history_pack(w);
    context * contextptr=hp?hp->contextptr:0;
    check_browser_help(g_,giac::language(contextptr));
    if (!hp)
      return 0;
    gen g=add_autosimplify(g_,contextptr);
    giac::gen evaled_g;
#ifdef EMCC2
    int hppos=hp->find(gr);
    //*logptr(contextptr) << "Xcas_eval " << g << '\n';
    evaled_g=protecteval(g,eval_level(contextptr),contextptr);
    //*logptr(contextptr) << "After_eval " << evaled_g << '\n';
    if (Log_Output * lout=find_log_output(gr))
      output_resize_parent(lout,false);
    Fl_Group * hpp = parent_skip_scroll(hp);
    if (evaled_g.type==_STRNG){
      int nl=1;
      const string & s=*evaled_g._STRNGptr;
      for (int i=0;i<s.size();++i){
        if (s[i]=='\n')
          ++nl;
      }
      Fl_Multiline_Output * o = new Fl_Multiline_Output(w->x(),w->y(),w->w(),nl*(w->labelsize()+7));
      o->textsize(w->labelsize());
      o->value(s.c_str());
      o->textcolor(FL_MAGENTA);
      fl_font(FL_HELVETICA,w->labelsize());
      return o;
    }
    if (Logo * logo=dynamic_cast<Logo *>(hpp)){
      return make_gen_output(w,evaled_g);
    }
    if (hpp){
      int N=hpp->children();
      for (int i=0;i<N;++i){
        Graph2d3d * geo = dynamic_cast<Graph2d3d *>(hpp->child(i));
        if (geo){
          geo->clear(hppos);
          geo->add(evaled_g);
          geo->no_handle=false;
          // hp->focus(hp_pos+1,false);
          return make_gen_output(w,evaled_g);
        }
      }
    }
    // *logptr(contextptr) << "Xcas_evaled " << g << '\n';
    Fl_Widget * wid=in_Xcas_eval(gr->child(gr->children()-1),evaled_g,1,contextptr);
    // if res!=0
    return wid;
#else
    // if w 2nd brother is a graph2d3d, return a graph2d3d with the same
    // config
    Fl_Widget * res = 0;
    if (gr && gr->children()>=3){
      if (Fl_Output * out=dynamic_cast<Fl_Output *>(gr->child(2))){
	if (strcmp(out->value(),gettext("Computing..."))==0){
	  out->value(gettext("Unable to launch thread. Press STOP to interrupt."));
	  return w;
	}
      }
      if (Fl_Group * grc2=dynamic_cast<Fl_Group * >(gr->child(2))){
	if (grc2->children()){
	  if (Graph2d3d * graph=dynamic_cast<Graph2d3d *>(grc2->child(0))){
	    Fl_Tile * temptile = new Fl_Tile(w->x(),w->y(),w->w(),w->labelsize()+6);
	    Graph2d * tmp = new Graph2d(w->x(),w->y(),w->w(),temptile->h(),"",hp);
	    temptile->end();
	    tmp->copy(*graph);
	    tmp->resize(tmp->x(),tmp->y(),graph->w(),tmp->h());
	    temptile->labelsize(w->labelsize());
	    res=temptile;
	  }
	}
      }
    }
    giac::history_in(contextptr).push_back(g_);
    // commented otherwise ans() does not work
    // giac::history_out.push_back(g);
    bool graphres=res;
    Fl_Output * out=0;
    out =new Fl_Output(w->x(),w->y(),w->w(),w->labelsize());
    out->labelsize(w->labelsize());
    if (!res)
      res=out;
    bool ok=make_thread(g,eval_level(contextptr),Xcas_eval_callback,res,contextptr);
    if (graphres){
      if (ok){
	gr->remove(out);
	delete out;
	return res;
      }
      gr->remove(res);
      delete res;
      res=out;
    }
    out->value(ok?gettext("Computing..."):gettext("Unable to launch thread. Press STOP to interrupt."));
    return res;
#endif
  }

  Fl_Widget * Xcas_eval(Fl_Widget * w) {
    giac::gen g;
    int res=parse(w,g);
    if (res==1)
      return Xcas_eval(w,g);
    if (res==-1)
      return w;
    /* Fl_Output * o = new Fl_Output(w->x(),w->y(),w->w(),w->h());
       o->value("Invalid input");
       return o; */
    return 0;
  }


  string print_DOUBLE_(double d){
    char s[256];
#ifdef IPAQ
    sprintf(s,"%.4g",d);
#else
    sprintf(s,"%.5g",d);
#endif
    return s;
  }

  string replaces(const string & s,char c1,const string & c2){
    string res;
    int l=s.size();
    res.reserve(l);
    const char * ch=s.c_str();
    for (int i=0;i<l;++i,++ch){
      if (*ch==c1) res+=c2; else res += *ch;
    }
    return res;
  }

  vecteur seq2vecteur(const vecteur & v){
    vecteur w(v);
    iterateur it=w.begin(),itend=w.end();
    for (;it!=itend;++it){
      if (it->type==_VECT)
	it->subtype=0;
    }
    return w;
  }

  History_Fold * load_history_fold(int sx,int sy,int sw,int sh,int sl,const char * filename,bool modified){
    Fl_Group::current(0);
    xcas::History_Fold * w = new xcas::History_Fold(sx,sy,sw,sh,1);
    w->end();
    w->pack->contextptr = giac::clone_context(giac::context0);
    w->pack->labelsize(sl);
    w->pack->eval=xcas::Xcas_eval;
    w->pack->_insert=xcas::Xcas_pack_insert;
    w->pack->_select=xcas::Xcas_pack_select;  
    w->pack->new_url(filename);
    w->pack->insert_url(filename,-1);
    w->labelfont(w->pack->labelfont());
    xcas::change_group_fontsize(w,w->pack->labelsize());
    if (!modified)
      w->pack->clear_modified();
    else {
      w->autosave(true);  
      if (w->pack->url){ delete w->pack->url; w->pack->url=0; }
      w->label("Unnamed");
    }
    return w;
  }

  std::string replace_html5(const string & s){
    string res;
    size_t ss=s.size(),i;
    for (i=0;i<ss;++i){
      char ch=s[i];
      if ( (ch>='0' && ch<='9') || (ch>='a' && ch<='z') || (ch>='A' && ch<='Z'))
	res += ch;
      else {
	res +='%';
	int t=(ch&0xf0)>>4;
	if (t<10) res += ('0'+t); else res += 'a'+(t-10);
	t=ch&0x0f;
	if (t<10) res += ('0'+t); else res += 'a'+(t-10);
      }
    }
    //std::cerr << s << '\n' << res << '\n';
    return res;
  }

  std::string widget_html5(const Fl_Widget * o,int & pos){
    string res;
    const giac::context * contextptr = get_context(o);
    // Add here code for specific widgets
    if (const Tableur_Group * t = dynamic_cast<const Tableur_Group *>(o)){
      Flv_Table_Gen * g=t->table;
      matrice m=g->m;
      res = replace_html5(gen(m,_SPREAD__VECT).print(contextptr));
      return '+'+res+'&';
    }
    if (const Figure * f = dynamic_cast<const Figure *>(o)){
#if 1
      History_Pack * hp=f->geo->hp;
      string s;
      for (int i=0;i<hp->children();++i){
        s += unlocalize(hp->value(i));
        if (!s.empty() && s[s.size()-1]!=';' && i!=hp->children()-1)
          s += ";";
        s += '\n';
      }
      res = replace_html5(s);
      int xpos=(pos%2)*400;
      int ypos=(pos/2)*400;
      ++pos;
      string spos=print_INT_(xpos)+","+print_INT_(ypos)+",";
      return "cas="+spos+res+'&';
#else
      res = widget_html5(f->geo->hp,pos);
#endif
      return res;
    }
    if (const Logo * l=dynamic_cast<const Logo *>(o)){
      res = widget_html5(l->hp,pos);
      return res;
    }
    if (const Editeur * ed=dynamic_cast<const Editeur *>(o)){
      string s=unlocalize(ed->value());
      res = replace_html5(s);
      return '+'+res+'&';
    }
    if (const Xcas_Text_Editor * ed=dynamic_cast<const Xcas_Text_Editor *>(o)){
      string s=unlocalize(ed->value());
      res = replace_html5(s);
      int xpos=(pos%2)*400;
      int ypos=(pos/2)*400;
      ++pos;
      string spos=print_INT_(xpos)+","+print_INT_(ypos)+",";
      switch (ed->pythonjs){
      case -1:
	return "js="+spos+res+'&';
      case 0: case 1: case 2:
	return "cas="+spos+res+'&';
      case 4:
	return "micropy="+spos+res+'&';
      }
      return '+'+res+'&';
    }
    if (dynamic_cast<const Fl_Output *>(o))
      return "";
    if (const Fl_Input_ * i=dynamic_cast<const Fl_Input_ *>(o)){
      string s=i->value();
      if (dynamic_cast<const Comment_Multiline_Input *>(i))
	s = "// "+replaces(s,'\n',"<br>");
      if ( dynamic_cast<const Multiline_Input_tab *>(i) )
	s=unlocalize(s);
      if (s.empty())
	return s;
      res = replace_html5(s);
      int xpos=(pos%2)*400;
      int ypos=(pos/2)*400;
      ++pos;
      string spos=print_INT_(xpos)+","+print_INT_(ypos)+",";
      return "cas="+spos+res+'&';
      return '+'+res+'&';
    }
    if (const Fl_Group * g=dynamic_cast<const Fl_Group *>(o)){
      int ypos=0;
      // call widget_sprint on children
      int n=g->children();
      for (int i=0;i<n;++i){
	Fl_Widget * wid=g->child(i);
	res += widget_html5(wid,pos);
      }
      return res;
    }
    if (const Gen_Value_Slider *g=dynamic_cast<const Gen_Value_Slider *>(o)){
      res = string(g->label())+","+print_DOUBLE_(g->value())+","+print_DOUBLE_(g->minimum())+","+print_DOUBLE_(g->maximum())+","+print_DOUBLE_(g->Fl_Valuator::step());
      return '*'+res+'&';
    }
    return res ;
  }

  std::string widget_sprint(const Fl_Widget * o,bool with_ans){
    string res;
    const giac::context * contextptr = get_context(o);
    res = "// fltk " + string(typeid(*o).name());
    int wh=o->h();
    if (const History_Fold * hf=dynamic_cast<const History_Fold *>(o)){
      if (hf->_folded)
	wh=hf->_horig;
    }
    bool With_ans=true; 
    const Fl_Tile * t=dynamic_cast<const Fl_Tile*>(o);
    if (!with_ans && t && (t->children()==2 || t->children()==3) ){
      With_ans=false; // print only 1st level
      res += " " + giac::print_INT_(t->x()) + " " + giac::print_INT_(t->y())+ " " + giac::print_INT_(t->w()) + " " + giac::print_INT_(t->child(0)->h()) + " " + giac::print_INT_(t->labelsize()) + " " + giac::print_INT_(t->labelfont()) ;
    }
    else
      res += " " + giac::print_INT_(o->x()) + " " + giac::print_INT_(o->y())+ " " + giac::print_INT_(o->w()) + " " + giac::print_INT_(wh) + " " + giac::print_INT_(o->labelsize()) + " " + giac::print_INT_(o->labelfont()) ;
    // Add here code for specific widgets
    if (const Flv_Table_Gen * g = dynamic_cast<const Flv_Table_Gen *>(o)){
      res += '\n'+print_INT_(g->is_spreadsheet?1:0)+" "+print_INT_(g->matrix_fill_cells?1:0)+" "+print_INT_(g->spreadsheet_recompute?1:0)+" "+print_INT_(g->matrix_symmetry?1:0)+ " " +replace(gen(g->m,_SPREAD__VECT).print(contextptr),'\n',char(0x7f))+'\n';
      return res;
    }
    if (const Tableur_Group * t = dynamic_cast<const Tableur_Group *>(o)){
      Flv_Table_Gen * g=t->table;
      Graph2d3d * gr = g->graph;
      matrice m=g->m;
      if (!g->m.empty()){
	gen m0=m.front();
	if (m0.type==_VECT && !m0._VECTptr->empty()){
	  vecteur m0v=*m0._VECTptr;
	  gen m00=m0v.front();
	  if (m00.type==_VECT && m00._VECTptr->size()==3){
	    vecteur rowv,colv,dispv;
	    for (int i=0;i<g->rows();++i)
	      rowv.push_back(g->row_height(i));
	    for (int i=0;i<g->cols();++i)
	      colv.push_back(g->col_width(i));
	    dispv.push_back(t->disposition);
	    if (t->disposition/2){
	      if (t->disposition % 2){ // graph at right
		dispv.push_back(g->w());
		dispv.push_back(g->graph->w());
		dispv.push_back(g->graph->mouse_param_group->w());
	      }
	      else { // graph below
		double dtable=double(g->h())/t->h();
		double dgraph=((1-dtable)*g->graph->w())/g->w();
		dispv.push_back(dtable);
		dispv.push_back(dgraph);
		dispv.push_back(1-dtable-dgraph);
	      }
	    }
	    gen newm002=makevecteur(g->name,
					  makevecteur(gr->window_xmin,gr->window_xmax,gr->window_ymin,gr->window_ymax),
					  gr->npixels,
					  makevecteur(gr->x_tick,gr->y_tick),
				    gr->show_axes,gr->show_names,dispv,makevecteur(rowv,colv),g->init);
	    m00=gen(makevecteur(m00[0],m00[1],newm002),m0v[0].subtype);
	    m0v[0]=m00;
	    m[0]=gen(m0v,m0[0].subtype);
	  }
	}
      }
      res += '\n'+print_INT_(g->is_spreadsheet)+" "+print_INT_(g->matrix_fill_cells)+" "+print_INT_(g->spreadsheet_recompute)+" "+print_INT_(g->matrix_symmetry)+ " " +replace(gen(m,_SPREAD__VECT).print(contextptr),'\n',char(0x7f))+'\n';
      return res;
    }
    if (const Figure * f = dynamic_cast<const Figure *>(o)){
      res += " landscape="+print_INT_(f->disposition)+" history=";
      if (f->disposition & 1){
	double tmp=double(f->geo->h())/f->h();
	res += print_DOUBLE_((1-tmp)*double(f->s->w())/f->w())+ " geo="+print_DOUBLE_(tmp*double(f->geo->w())/f->w())+ " "+" mouse_param="+print_DOUBLE_(tmp*double(f->geo->mouse_param_group->w())/f->w());
      }
      else
	res += print_DOUBLE_(double(f->s->w())/f->w())+ " geo="+print_DOUBLE_(double(f->geo->w())/f->w())+ " "+" mouse_param="+print_DOUBLE_(double(f->geo->mouse_param_group->w())/f->w());
      res += '\n'+widget_sprint(f->geo->hp,with_ans)+widget_sprint(f->geo,with_ans)+'\n';
      return res;
    }
    if (const Logo * l=dynamic_cast<const Logo *>(o)){
      res += '\n'+widget_sprint(l->hp,with_ans)+widget_sprint(l->ed,with_ans);
      return res;
    }
    if (const Editeur * ed=dynamic_cast<const Editeur *>(o)){
      string s=unlocalize(ed->value());
      res += '\n'+print_INT_(s.size())+" "+print_INT_(ed->editor->pythonjs);
      if (ed->output)
        res += " "+string(ed->output->value());
      res += " ,\n"+s;
      return res;
    }
    if (const Xcas_Text_Editor * ed=dynamic_cast<const Xcas_Text_Editor *>(o)){
      string s=unlocalize(ed->value());
      res += '\n'+print_INT_(s.size())+" "+print_INT_(ed->pythonjs)+" ,\n"+s;
      return res;
    }
    if (const Gen_Output * i=dynamic_cast<const Gen_Output *>(o)){
      res += '\n';
      string s=taille(i->value(),100)>100?string("Done"):i->value().print(contextptr);
      s=unlocalize(s);
      res += replace(s,'\n',char(0x7f));
      // res += '"';
      return res + '\n';
    }
    if (const Fl_Input_ * i=dynamic_cast<const Fl_Input_ *>(o)){
      res += '\n';
      string s=i->value();
      if ( dynamic_cast<const Multiline_Input_tab *>(i) )
	s=unlocalize(s);
      res += replace(s,'\n',char(0x7f));
      // res += '"';
      return res + '\n';
    }
    if (const Log_Output * i=dynamic_cast<const Log_Output *>(o)){
      res += '\n';
      // res += '"';
      res += replace(i->value(),'\n',char(0x7f));
      // res += '"';
      return res + '\n';
    }
    if (const Equation * i=dynamic_cast<const Equation *>(o)){
      res += " " + giac::print_INT_(i->output_equation);
      res += '\n';
      // res += '"';
      string s=taille(i->get_data(),1000)>1000?string("Done"):i->value();
      res += replace(unlocalize(s),'\n',char(0x7f));
      // res += '"';
      return res + '\n';
    }
    if (const Graph2d3d * i=dynamic_cast<const Graph2d3d *>(o)){
      res += '\n';
      res += print_DOUBLE_(i->window_xmin) + ',' + print_DOUBLE_(i->window_xmax) + ',';
      res += print_DOUBLE_(i->window_ymin) + ',' + print_DOUBLE_(i->window_ymax) + ',';
      if (with_ans)
        res += replace(giac::gen(giac::merge_pixon(seq2vecteur(i->plot_instructions))).print(contextptr),'\n',char(0x7f));
      else
        res += "[]";
      res += ','+ print_DOUBLE_(i->window_zmin) + ',' + print_DOUBLE_(i->window_zmax)+','+print_DOUBLE_(i->q.w) +','+print_DOUBLE_(i->q.x)+','+print_DOUBLE_(i->q.y)+','+print_DOUBLE_(i->q.z) + ','+print_DOUBLE_(i->x_tick) + ',' + print_DOUBLE_(i->y_tick)+','+print_INT_(i->show_axes)+','+print_INT_(i->couleur)+','+print_INT_(i->approx)+','+print_DOUBLE_(i->ylegende)+',';
      if (i->paused)
	res += "-";
      res +=print_DOUBLE_(i->animation_dt)+','+print_INT_(i->show_mouse_on_object)+','+print_INT_(i->display_mode);
      res += ",[";
      if (const Graph3d* gr3d=dynamic_cast<const Graph3d *>(i)){
	for (int j=0;;){
	  res += "["+ print_DOUBLE_(i->light_x[j]);
	  res += ","+ print_DOUBLE_(i->light_y[j]);
	  res += ","+ print_DOUBLE_(i->light_z[j]);
	  res += ","+ print_DOUBLE_(i->light_w[j]);
	  res += ","+ print_DOUBLE_(i->light_diffuse_r[j]);
	  res += ","+ print_DOUBLE_(i->light_diffuse_g[j]);
	  res += ","+ print_DOUBLE_(i->light_diffuse_b[j]);
	  res += ","+ print_DOUBLE_(i->light_diffuse_a[j]);
	  res += ","+ print_DOUBLE_(i->light_specular_r[j]);
	  res += ","+ print_DOUBLE_(i->light_specular_g[j]);
	  res += ","+ print_DOUBLE_(i->light_specular_b[j]);
	  res += ","+ print_DOUBLE_(i->light_specular_a[j]);
	  res += ","+ print_DOUBLE_(i->light_ambient_r[j]);
	  res += ","+ print_DOUBLE_(i->light_ambient_g[j]);
	  res += ","+ print_DOUBLE_(i->light_ambient_b[j]);
	  res += ","+ print_DOUBLE_(i->light_ambient_a[j]);
	  res += ","+ print_DOUBLE_(i->light_spot_x[j]);
	  res += ","+ print_DOUBLE_(i->light_spot_y[j]);
	  res += ","+ print_DOUBLE_(i->light_spot_z[j]);
	  res += ","+ print_DOUBLE_(i->light_spot_w[j]);
	  res += ","+ print_DOUBLE_(i->light_spot_exponent[j]);
	  res += ","+ print_DOUBLE_(i->light_spot_cutoff[j]);
	  res += ","+ print_DOUBLE_(i->light_0[j]);
	  res += ","+ print_DOUBLE_(i->light_1[j]);
	  res += ","+ print_DOUBLE_(i->light_2[j])+",";
	  if (!gr3d->opengl)
	    res += (i->light_on[j]?"3":"2");
	  else
	    res += (i->light_on[j]?"1":"0");
	  res +="]";
	  ++j;
	  if (j==8)
	    break;
	  res += ",";
	}
      } // end i is a graph3d
      res += "]";
      res += ","+print_INT_(i->ntheta)+","+print_INT_(i->nphi);
      res += ","+print_INT_(i->rotanim_type)+","+print_INT_(i->rotanim_danim)+","+print_INT_(i->rotanim_nstep)+","+print_DOUBLE_(i->rotanim_rx)+","+print_DOUBLE_(i->rotanim_ry)+","+print_DOUBLE_(i->rotanim_rz)+","+print_DOUBLE_(i->rotanim_tstep);
      res += ","+print_INT_(i->x_axis_color)+","+print_INT_(i->y_axis_color)+","+print_INT_(i->z_axis_color);
      return res + '\n';
    }
    if (const History_Fold * g=dynamic_cast<const History_Fold *>(o)){
      res += '\n'+replace(g->input->value(),'\n',char(0x7f)) + '\n';
      res += widget_sprint(g->pack,with_ans);
      return res;
    }
    if (const Fl_Group * g=dynamic_cast<const Fl_Group *>(o)){
      int ypos=0;
      if (Fl_Scroll * s=(Fl_Scroll *) dynamic_cast<const Fl_Scroll *>(g)){
	ypos=s->yposition();
#ifdef _HAVE_FL_UTF8_HDR_
	s->scroll_to(s->xposition(),0);
#else
	s->position(s->xposition(),0);
#endif
      }
      // call widget_sprint on children
      int n=g->children();
      res += "\n[\n";
      for (int i=0;i<n;++i){
	Fl_Widget * wid=g->child(i);
	res += widget_sprint(wid,with_ans);
        if (!With_ans)
          break;
	if (i!=n-1)
	  res += ",\n";
      }
      res += "]\n";
      if (Fl_Scroll * s=(Fl_Scroll *) dynamic_cast<const Fl_Scroll *>(g)){
#ifdef _HAVE_FL_UTF8_HDR_
	s->scroll_to(s->xposition(),ypos);
#else
	s->position(s->xposition(),ypos);
#endif
      }
      return res;
    }
    if (const Gen_Value_Slider *g=dynamic_cast<const Gen_Value_Slider *>(o)){
      res += "\n" + giac::print_INT_(g->pos)+" "+print_DOUBLE_(g->minimum())+" "+print_DOUBLE_(g->maximum())+" "+print_DOUBLE_(g->value())+" "+string(g->label())+" "+print_DOUBLE_(g->Fl_Valuator::step())+"\n";
      return res;
    }
    return res + "\n[]\n";
  }

  void next_line(const string & s,int L,string & line,int & i){
    line="";
    for (;i<L;++i){
      if (0 && i<L-1 && s[i]==0xd && s[i+1]==0xa){ 
	// windows CR should not happen anymore
	line += '\n';
	i+=2;
	break;
      }
      line += ( (s[i]==char(0x7f) || s[i]==char(0243))?'\n':s[i]);
      if (s[i]=='\n'){
	++i;
	break;
      }
    }
    // cerr << i << " " << line << '\n';
  }

  void next_line_nonl(const string & s,int L,string & line,int & i){
    next_line(s,L,line,i);
    int t=line.size();
    if (t && line[t-1]=='\n')
      line=line.substr(0,t-1);
  }

  // Read a group of widget from string s starting a pos i
  Fl_Group * widget_group_load(Fl_Group * res,const string & s,int L,int & i,GIAC_CONTEXT){
    Fl_Group::current(res);
    for (;Fl_Widget * o=widget_load(s,L,i,contextptr,res->w());){
      if (History_Pack * hp=dynamic_cast<History_Pack *>(o))
	hp->contextptr=(giac::context *)contextptr;
      if (o->w()>res->w())
	o->resize(o->x(),o->y(),res->w(),o->h());
      if (o->parent()!=res)
	res->add(o);
      // remove empty group
      if (Fl_Group * gr = dynamic_cast<Fl_Group * >(o)){
	if (!gr->children())
	  res->remove(gr);
      }
      if (i<L-1 && s[i]==',' && s[i+1]=='\n')
	i += 2;
    }
    res->end();
    string line;
    next_line(s,L,line,i);
    if (line=="]\n")
      return res;
    else
      return 0;
  }

  void graphic_load(Graph2d3d * res,const std::string & s,int L,string & line,int & i){
    /*
    if (res->mouse_param_group && !dynamic_cast<Figure *>(res->parent())){
      int X=res->x();
      int Y=res->y();
      int W=res->parent()->w();
      int H=res->h();
      int l=W/4;
      if (l>6*res->labelsize())
	l=6*res->labelsize();
      res->resize(X,Y,W-l,H);
      res->resize_mouse_param_group(l);
    }
    */
    next_line_nonl(s,L,line,i);
    if (s[i]=='\n')
      ++i;
    const giac::context * contextptr = get_context(res);
    giac::gen g(line,contextptr);
    if (g.type==_VECT && g._VECTptr->size()>=5){
      giac::vecteur & v =*g._VECTptr;
      res->window_xmin= giac::evalf_double(v[0],1,contextptr)._DOUBLE_val;
      res->window_xmax= giac::evalf_double(v[1],1,contextptr)._DOUBLE_val;
      res->window_ymin= giac::evalf_double(v[2],1,contextptr)._DOUBLE_val;
      res->window_ymax= giac::evalf_double(v[3],1,contextptr)._DOUBLE_val;
      gen gg=v[4];
      if (gg.type==_VECT)
	res->add(*gg._VECTptr);
      else
	res->add(gg);
      if (v.size()>=7){
	res->window_zmin= giac::evalf_double(v[5],1,contextptr)._DOUBLE_val;
	res->window_zmax= giac::evalf_double(v[6],1,contextptr)._DOUBLE_val;
      }
      if (v.size()>=11){
	res->q=quaternion_double(giac::evalf_double(v[7],1,contextptr)._DOUBLE_val,giac::evalf_double(v[8],1,contextptr)._DOUBLE_val,giac::evalf_double(v[9],1,contextptr)._DOUBLE_val,giac::evalf_double(v[10],1,contextptr)._DOUBLE_val);
      }
      if (v.size()>=13){
	res->x_tick=giac::evalf_double(v[11],1,contextptr)._DOUBLE_val;
	res->y_tick=giac::evalf_double(v[12],1,contextptr)._DOUBLE_val;	
      }
      if (v.size()>=14)
	res->show_axes=v[13].val;
      if (v.size()>=16){
	res->couleur=v[14].val;
	res->approx=v[15].val;
      }
      if (v.size()>=17)
	res->ylegende=evalf_double(v[16],1,contextptr)._DOUBLE_val;
      if (v.size()>=18){
	gen tmp=evalf_double(v[17],1,contextptr);
	res->animation_dt=tmp.DOUBLE_val();
	if (std::abs(res->animation_dt<1e-10))
	  res->animation_dt=0;
	if (res->animation_dt<0){
	  res->paused=true;
	  res->animation_dt=-res->animation_dt;
	}
      }
      if (v.size()>=19)
	res->show_mouse_on_object=int(evalf_double(v[18],1,contextptr)._DOUBLE_val);
      if (v.size()>=20)
	res->display_mode=int(evalf_double(v[19],1,contextptr)._DOUBLE_val);
      if (v.size()>=21 && v[20].type==_VECT){
	vecteur & vv=*v[20]._VECTptr;
	for (unsigned j=0;j< vv.size() && j<8;++j){
	  gen & tmp=vv[j];
	  if ( (tmp.type==_VECT && tmp._VECTptr->size()>=25) ){
	    vecteur w = *evalf_double(tmp,1,contextptr)._VECTptr;
	    res->light_x[j]=w[0]._DOUBLE_val;
	    res->light_y[j]=w[1]._DOUBLE_val;
	    res->light_z[j]=w[2]._DOUBLE_val;
	    res->light_w[j]=w[3]._DOUBLE_val;
	    res->light_diffuse_r[j]=w[4]._DOUBLE_val;
	    res->light_diffuse_g[j]=w[5]._DOUBLE_val;
	    res->light_diffuse_b[j]=w[6]._DOUBLE_val;
	    res->light_diffuse_a[j]=w[7]._DOUBLE_val;
	    res->light_specular_r[j]=w[8]._DOUBLE_val;
	    res->light_specular_g[j]=w[9]._DOUBLE_val;
	    res->light_specular_b[j]=w[10]._DOUBLE_val;
	    res->light_specular_a[j]=w[11]._DOUBLE_val;
	    res->light_ambient_r[j]=w[12]._DOUBLE_val;
	    res->light_ambient_g[j]=w[13]._DOUBLE_val;
	    res->light_ambient_b[j]=w[14]._DOUBLE_val;
	    res->light_ambient_a[j]=w[15]._DOUBLE_val;
	    res->light_spot_x[j]=w[16]._DOUBLE_val;
	    res->light_spot_y[j]=w[17]._DOUBLE_val;
	    res->light_spot_z[j]=w[18]._DOUBLE_val;
	    res->light_spot_w[j]=w[19]._DOUBLE_val;
	    res->light_spot_exponent[j]=w[20]._DOUBLE_val;
	    res->light_spot_cutoff[j]=w[21]._DOUBLE_val;
	    res->light_0[j]=w[22]._DOUBLE_val;
	    res->light_1[j]=w[23]._DOUBLE_val;
	    res->light_2[j]=w[24]._DOUBLE_val;
	    if (tmp._VECTptr->size()>=26){
	      int i=int(w[25]._DOUBLE_val);
	      if (i==1 || i==3)
		res->light_on[j]=i;
	      if (i==2 || i==3) {
		if (Graph3d* gr3d=dynamic_cast<Graph3d*>(res))
		  gr3d->opengl=false;
	      }
	    }
	  }
	}
      }
      if (v.size()>=23 && v[21].type==_INT_ && v[22].type==_INT_){
	res->ntheta=v[21].val;
	res->nphi=v[22].val;
      }
      if (v.size()>=26 && v[23].type==_INT_ && v[24].type==_INT_&& v[25].type==_INT_){
	res->rotanim_type=v[23].val;
	res->rotanim_danim=v[24].val;
	res->rotanim_nstep=v[25].val;
      }
      if (v.size()>=30 && v[26].type==_DOUBLE_ && v[27].type==_DOUBLE_&& v[28].type==_DOUBLE_){
	res->rotanim_rx=v[26]._DOUBLE_val;
	res->rotanim_ry=v[27]._DOUBLE_val;
	res->rotanim_rz=v[28]._DOUBLE_val;
	res->rotanim_tstep=v[29]._DOUBLE_val;
      }
      if (v.size()>=33 && v[30].type==_INT_ && v[31].type==_INT_&& v[32].type==_INT_){
	res->x_axis_color=v[30].val;
	res->y_axis_color=v[31].val;
	res->z_axis_color=v[32].val;
      }
    }
    if (Geo2d * geo=dynamic_cast<Geo2d *>(res)){
      geo->hp=geo_find_history_pack(geo);
      if (geo_run && geo->hp) 
	geo->hp->update();
    }
#if 1 // def HAVE_LIBFLTK_GL
    if (Geo3d * geo=dynamic_cast<Geo3d *>(res)){
      geo->hp=geo_find_history_pack(geo);
      if (geo_run && geo->hp) 
	geo->hp->update();
    }
#endif
  }

  void tableur_load(Flv_Table_Gen * & res,const std::string & s,int x,int y,int w,int h){
    int taille=s.size(),i=0;
    for (int j=0;j<4&&i<taille;i++){
      j += (s[i]==' ');
    }
    string s1=s.substr(0,i);
    string s2=s.substr(i,taille-i);
#ifdef HAVE_SSTREAM
    istringstream in(s1);
#else
    istrstream in(s1.c_str());
#endif
    int is_spreadsheet,matrix_fill_cells,spreadsheet_recompute,matrix_symmetry;
    in >> is_spreadsheet >> matrix_fill_cells >> spreadsheet_recompute >> matrix_symmetry ;
    giac::context * contextptr=get_context(res);
    gen g(s2,contextptr);
    if (!ckmatrix(g,true))
      return ;
    Tableur_Group * t=dynamic_cast<Tableur_Group *>(res->parent());
    spread_ck(*g._VECTptr); // in place modifications of g
    // Find name of tableur in g
    matrice m=*g._VECTptr;
    if (!res)
      res = new Flv_Table_Gen(x,y,w,h,"");
    else {
      int n,c;
      mdims(m,n,c);
      res->rows(n);
      res->cols(c);
    }
    if (!m.empty()){
      gen g0=g._VECTptr->front();
      if (g0.type==_VECT && !g0._VECTptr->empty()){
	vecteur m0=*g0._VECTptr;
	gen g00= m0.front();
	if (g00.type==_VECT && g00._VECTptr->size()>=3){
	  vecteur m00=*g00._VECTptr;
	  gen g002 = m00.back();
	  if (g002.type==_IDNT)
	    res->name=g002;
	  if (g002.type==_VECT){
	    Graph2d * gr = res->graph;
	    vecteur & ggv = *g002._VECTptr;
	    int ggs=ggv.size();
	    if (ggs>0 && ggv[0].type==_IDNT)
	      res->name=ggv[0];
	    if (gr && ggs>5){
	      if (ggv[1].type==_VECT && ggv[1]._VECTptr->size()>3){
		vecteur & tmpv=*ggv[1]._VECTptr;
		gr->window_xmin=evalf_double(tmpv[0],1,contextptr)._DOUBLE_val;
		gr->window_xmax=evalf_double(tmpv[1],1,contextptr)._DOUBLE_val;
		gr->window_ymin=evalf_double(tmpv[2],1,contextptr)._DOUBLE_val;
		gr->window_ymax=evalf_double(tmpv[3],1,contextptr)._DOUBLE_val;
	      }
	      if (ggv[2].type==_INT_)
		gr->npixels = ggv[2].val;
	      if (ggv[3].type==_VECT && ggv[3]._VECTptr->size()>1){
		vecteur & tmpv=*ggv[3]._VECTptr;
	      gr->x_tick=evalf_double(tmpv[0],1,contextptr)._DOUBLE_val;
	      gr->y_tick=evalf_double(tmpv[1],1,contextptr)._DOUBLE_val;
	      }
	      if (ggv[4].type==_INT_)
		gr->show_axes = ggv[4].val;
	      if (ggv[5].type==_INT_)
		gr->show_names = ggv[5].val;
	    }
	    if (ggs>6 && t){
	      if (ggv[6].type==_INT_)
		t->disposition=ggv[6].val;
	      if (ggv[6].type==_VECT){
		vecteur dispv=*ggv[6]._VECTptr;
		int dispvs=dispv.size();
		if (dispvs)
		  t->disposition=dispv[0].val;
		if (dispvs>3){
		  double d1=evalf_double(dispv[1],1,contextptr)._DOUBLE_val;
		  double d2=evalf_double(dispv[2],1,contextptr)._DOUBLE_val;
		  double d3=evalf_double(dispv[3],1,contextptr)._DOUBLE_val;
		  t->resize2(d1,d2,d3);
		}
	      }
	    }
	    if (ggs>7 && t){ // read row_height and col_witdh
	      if (ggv[7].type==_VECT && ggv[7]._VECTptr->size()==2){
		gen & rowg=ggv[7]._VECTptr->front();
		if (rowg.type==_VECT){
		  vecteur & rowv = *rowg._VECTptr;
		  int rowvs=giacmin(rowv.size(),t->table->rows());
		  for (int i=0;i<rowvs ;++i){
		    if (rowv[i].type==_INT_)
		      t->table->row_height(rowv[i].val,i);
		  }
		}
		gen & colg=ggv[7]._VECTptr->back();
		if (colg.type==_VECT){
		  vecteur & colv = *colg._VECTptr;
		  int colvs=giacmin(colv.size(),t->table->cols());
		  for (int i=0;i<colvs ;++i){
		    if (colv[i].type==_INT_)
		      t->table->col_width(colv[i].val,i);
		  }
		}
	      }
	    }
	    if (ggs>8 && t){
	      t->table->init=ggv[8];
	    }
	  } // end g002 of type _VECT
	  g002=2;
	  m00.back()=g002;
	  m0.front()=gen(m00,m0.front().subtype);
	  m.front()=gen(m0,m.front().subtype);
	  res->filename = new string(res->name.print(contextptr)+".tab");
	}
      }
    }
    if (res)
      res->set_matrix(m,false,false);
    res->is_spreadsheet = is_spreadsheet;
    res->matrix_fill_cells = matrix_fill_cells;
    res->spreadsheet_recompute = spreadsheet_recompute;
    res->matrix_symmetry=matrix_symmetry;
    res->update_status();
    if (sheet_run)
      res->spread_eval_interrupt();
    res->update_spread_graph();
    res->redraw();
    if (t)
      t->resize2();
  }

  void xcas_text_editor_load(Xcas_Text_Editor * & res,const std::string & s,int x,int y,int w,int h){
    Fl_Text_Buffer * b = new Fl_Text_Buffer;
    res = new Xcas_Text_Editor(x,y,w,h,b);
    res->callback(History_Pack_cb_eval,0);
    res->textcolor(Xcas_input_color);
    res->color(Xcas_input_background_color);
    res->buffer()->add_modify_callback(style_update, res); 
    res->buffer()->insert(0,s.c_str());
    res->set_gchanged();
    res->end();
  }

  void editeur_load(Editeur * & res,const std::string & s,int x,int y,int w,int h){
    res = new Editeur(x,y,w,h,"");
    res->callback(History_Pack_cb_eval,0);
    res->editor->buffer()->insert(0,s.c_str());
    if (s.size()>=7 && s.substr(0,7)=="#xwaspy")
      res->editor->locked=true;
  }

  bool split_string(const string & s,char sep,string & before,string & after){
    int j=s.find('='),ss=s.size();
    if (j>0 && j<ss-1){
      before=s.substr(0,j);
      after=s.substr(j+1,ss-j-1);
      return true;
    }
    else
      return false;
  }

  void read_size_mode(const string & line, int & taille,int & mode,string & filename){
    filename="";
    taille=mode=0;
    int i=0,n=line.size();
    for (;i<n;++i){
      if (line[i]<'0' || line[i]>'9')
	break;
      taille *= 10;
      taille += line[i]-'0';
    }
    ++i;
    if (i>=n) return;
    bool neg=false;
    if (line[i]=='-'){
      ++i;
      neg=true;
    }
    for (;i<n;++i){
      if (line[i]<'0' || line[i]>'9')
	break;
      mode *= 10;
      mode += line[i]-'0';
    }
    if (neg) mode=-mode;
    for (;i<n;++i){
      if (line[i]!=' ') break;
    }
    for (;i<n;++i){
      if (my_isalpha(line[i]) || line[i]=='.')
	filename += line[i];
    }
  }

  // Read a widget from string s starting at position i
  // Return 0 on syntax error or incomplete widget otherwise
  Fl_Widget * widget_load(const std::string & s,int L,int & i,GIAC_CONTEXT,int widgetw){
    // Get next line
    string line;
    int i_orig=i;
    next_line(s,L,line,i);
    // Check for a fltk comment
    if (line.size()>8 && line.substr(0,8)=="// fltk "){
#ifdef HAVE_SSTREAM
    istringstream is(line);
#else
    istrstream is(line.c_str());
#endif
      string tmp,attrib;
      char ch;
      is >> tmp; // get //
      is >> tmp; // get fltk
      is >> tmp; // get widget type
      vector<string> v;
      for (;;){
	ch=is.get();
	if (ch=='\n' || !is || is.eof())
	  break;
	if (ch<=' '){
	  if (!attrib.empty()){
	    v.push_back(attrib);
	    attrib="";
	  }
	}
	else
	  attrib += ch;
      }
      if (!attrib.empty())
	v.push_back(attrib);
      int vs=v.size();
      int x=(vs>0)?atoi(v[0].c_str()):0;
      int y=(vs>1)?atoi(v[1].c_str()):0;
      int w=(vs>2)?atoi(v[2].c_str()):10;
      bool ortho=false;
      if (widgetw){
	ortho=(w!=widgetw);
	w=widgetw;
      }
      if (w<10)
	w=10;
      int h0=(vs>3)?atoi(v[3].c_str()):5;
      if (h0<1)
	h0=1;
      int h=h0;
      if (h<5)
	h=5;
      int lsize=(vs>4)?atoi(v[4].c_str()):14;
      Fl_Font police=(vs>5)?Fl_Font(atoi(v[5].c_str())):FL_HELVETICA;
      if (police>=fonts_available)
	police=FL_HELVETICA;
      unsigned pos=tmp.find("History_Fold"),tmps=tmp.size();
      if (pos>0 && pos<tmps){
	next_line_nonl(s,L,line,i);
	History_Fold * res= new History_Fold(x,y,w,h);
	res->labelsize(lsize);
	res->labelfont(police);
	res->input->value(line.c_str());
	next_line(s,L,line,i); // should be history_pack...
	next_line(s,L,line,i); // should be [\n
	res->pack->labelfont(police);
	res->pack->eval=xcas::Xcas_eval;
	res->pack->_insert=xcas::Xcas_pack_insert;
	res->pack->_select=xcas::Xcas_pack_select;
	widget_group_load(res->pack,s,L,i,contextptr);
	res->fold();
	res->pack->_resize_above=true;
	res->redraw();
	return res->pack?res:0;
      }
      pos = tmp.find("History_Pack");
      if (pos>0 && pos<tmps){ 
	History_Pack * res= new History_Pack(x,y,w,h);
	res->labelsize(lsize);
	res->labelfont(police);
	next_line(s,L,line,i); // should be [\n
	res->eval=xcas::Xcas_eval;
	res->_insert=xcas::Xcas_pack_insert;
	res->_select=xcas::Xcas_pack_select;
	widget_group_load(res,s,L,i,contextptr);
	res->redraw();
	// next_line(s,L,line,i);
	return res;
      }
      pos = tmp.find("Tableur_Group");
      if (pos>0 && pos<tmps){ 
	Tableur_Group * res= new Tableur_Group(x,y,w,h,lsize);
	res->labelfont(police);
	next_line_nonl(s,L,line,i);
	tableur_load(res->table,line,x,y,w,h);
	return res;
      }
      pos=tmp.find("Turtle");
      if (pos>0 && pos<tmps){ 
	Turtle * tu=new Turtle(x,y,w,h);
	tu->turtleptr=&turtle_stack(context0);
	next_line(s,L,line,i); // skip []
	return tu;
      }
      pos=tmp.find("Fl_Scrollbar");
      if (pos>0 && pos<tmps){ 
	next_line(s,L,line,i); // skip []
	return 0;
      }
      pos=tmp.find("Mouse_Position");
      if (pos>0 && pos<tmps){ 
	next_line(s,L,line,i); // skip []
	return new Mouse_Position(x,y,w,h,0);
      }
      pos=tmp.find("Fl_Button");
      unsigned pos2=tmp.find("Fl_Menu_Bar");
      if ( (pos>0 && pos<tmps) || (pos2>0 && pos2<tmps) ){ 
	next_line(s,L,line,i); // skip []
	return new Fl_Button(x,y,w,h);
      }
      pos=tmp.find("Scroll");
      if (pos>0 && pos<tmps){
	next_line(s,L,line,i);
	if (line!="[\n")
	  return 0;
	Fl_Scroll * res= new Fl_Scroll(x,y,w,h);
	res->labelsize(lsize);
	res->labelfont(police);
	res->box(FL_FLAT_BOX);
	widget_group_load(res,s,L,i,contextptr);
	// reorder scrollbars at the end
	int n=res->children();
	if (n && dynamic_cast<Fl_Scrollbar *>(res->child(0)))
	  res->add(res->child(0));
	if (n && dynamic_cast<Fl_Scrollbar *>(res->child(0)))
	  res->add(res->child(0));
	// skip next scrollbar
	next_line(s,L,line,i); // skip Fl_Scrollbar
	next_line(s,L,line,i); // skip []
	next_line(s,L,line,i); // skip ] (matching the [ from Fl_Scroll)
	return res;
      }      
      pos=tmp.find("Fl_Group");
      if (pos>0 && pos<tmps){
	next_line(s,L,line,i);
	if (line!="[\n")
	  return 0;
	int i2=i;
	string line2;
	next_line(s,L,line2,i2);
	Fl_Group * res= new Fl_Group(x,y,w,h);
	res->labelsize(lsize);
	res->labelfont(police);
	widget_group_load(res,s,L,i,contextptr);
	tmps=line2.size();
	pos=line2.find("Mouse_Position");
	if (pos>0 && pos <tmps)
	  res->clear(); // remove this group since it's a param_mouse_group
	return res;
      }
      pos=tmp.find("Fl_Tile");
      if (pos>0 && pos<tmps){
	next_line(s,L,line,i);
	if (line!="[\n")
	  return 0;
	int i2=i;
	string line2;
	next_line(s,L,line2,i2);
	Fl_Tile * res= new Fl_Tile(x,y,w,h);
	res->labelsize(lsize);
	res->labelfont(police);
	widget_group_load(res,s,L,i,contextptr);
	tmps=line2.size();
	pos=line2.find("Mouse_Position");
	if (pos>0 && pos <tmps)
	  res->clear(); // remove this group since it's a param_mouse_group
	pos=line2.find("Flv_Table_Gen");
	// remove objects not linked inside Flv_Table_Gen
	if (pos>0 && pos<tmps){
	  int n=res->children();
	  Flv_Table_Gen * g = dynamic_cast<Flv_Table_Gen *>(res->child(0));
	  if (g){
	    for (int i=n-1;i;--i){
	      Fl_Widget * w = res->child(i);
	      if (w==g->_goto || w==g->input || w==g->graph || w==g->graph-> mouse_param_group)
		break;
	      res->remove(w);
	      delete w;
	    }
	  }
	}
	return res;
      }
      pos=tmp.find("Figure");
      if (pos>0 && pos<tmps){
	next_line(s,L,line,i); // should be History_Pack
	next_line(s,L,line,i); // should be [\n
	Figure * fig = new Figure(x,y,w,h,lsize,false);
	fig->geo->no_handle=true;
	fig->geo->hp->remove_entry(0,false);
	fig->geo->hp->eval_below=true;
	fig->geo->hp->pretty_output=false;
	fig->geo->hp->labelfont(police);
	widget_group_load(fig->geo->hp,s,L,i,contextptr);
	next_line(s,L,line,i); 
	pos=line.find("3d");// should be Geo2d or Geo3d
	bool dim3= pos>0 && pos<line.size();
#if 1 // def HAVE_LIBFLTK_GL
	if (dim3){
	  Graph2d3d * wid=fig->geo;
	  Fl_Group * p=wid->parent();
	  p->remove(wid);
	  p->remove(wid->mouse_param_group);
	  Fl_Group::current(p);
	  fig->geo=new Geo3d(wid->x(),wid->y(),2*w/3,h-wid->labelsize(),fig->geo->hp);
	  int pos=p->find(wid);
	  p->insert(*fig->geo,pos);
	  delete wid;
	}
#endif
	graphic_load(fig->geo,s,L,line,i);
	// fig->geo->hide();
	// parse further parameters for the figure
	string before,after;
	double dhp=0.25,dgeo=0.5,dmp=0.25;
	for (int i=5;i<vs;++i){
	  if (split_string(v[i],'=',before,after)){
	    if (before=="landscape" && after=="1"){
	      fig->disposition=1;
	    }
	    if (before=="history") // percentage for History Pack
	      dhp=max(0.14,atof(after.c_str()));
	    if (before=="geo") 
	      dgeo=max(0.28,atof(after.c_str()));
	    if (before=="mouse_param")
	      dmp=max(0.14,atof(after.c_str()));
	  }
	}
	fig->resize(fig->x(),fig->y(),fig->w(),fig->h(),dhp,dgeo,dmp);
	if (ortho)
	  fig->geo->orthonormalize();
	fig->geo->hp->resize();
	fig->redraw();
	return fig;
      }
      pos=tmp.find("Logo");
      if (pos>0 && pos<tmps){
	Logo * l = new Logo(x,y,w,h,lsize);
	l->hp->remove_entry(0);
	next_line(s,L,line,i); // should be History_Pack
	next_line(s,L,line,i); // should be [\n
	widget_group_load(l->hp,s,L,i,contextptr);
	l->hp->resize();
	turtle(contextptr).widget = l->t;
	next_line_nonl(s,L,line,i); // should be Editeur
	next_line_nonl(s,L,line,i);
	// read size
#ifdef HAVE_SSTREAM
    istringstream is(line);
#else
    istrstream is(line.c_str());
#endif
	int taille;
	is >> taille;
	l->ed->editor->buffer()->remove(0,l->ed->editor->buffer()->length());	
	l->ed->editor->buffer()->insert(0,s.substr(i,taille).c_str());
	// cb_Editeur_Exec_All(l->ed->editor,0);
	i += taille ;
	return l;
      }
      pos= tmp.find("Equation");
      if (pos>0 && pos<tmps){ 
	next_line_nonl(s,L,line,i);
	giac::gen g(line,contextptr);
	bool output=true;
	if (vs>6)
	  output=atoi(v[6].c_str());
	giac::attributs attr(lsize,output?Xcas_equation_background_color:Xcas_equation_input_background_color,output?Xcas_equation_color:Xcas_equation_input_color);
	Equation * res = new Equation(x,y,w,h,"",g,attr);
	res->box(FL_FLAT_BOX);
	res->labelsize(lsize);
	res->labelfont(police);
	res->output_equation=output;
	if (!output)
	  res->cb_enter=History_Pack_cb_eval;
	return res;
      }
      pos = tmp.find("Flv_Table_Gen");
      if (pos>0 && pos<tmps){ 
	Flv_Table_Gen * res=0;
	next_line_nonl(s,L,line,i);
	tableur_load(res,line,x,y,w,h);
	return res;
      }
      pos = tmp.find("Xcas_Text_Editor");
      if (pos>0 && pos<tmps){ 
	Xcas_Text_Editor * res=0;
	next_line_nonl(s,L,line,i);
	// read size
	int taille,mode=python_compat(contextptr); string filename;
	read_size_mode(line,taille,mode,filename);
	string tmp=s.substr(i,taille);
	if (mode==0)
	  tmp=localize(tmp,language(contextptr));
	xcas_text_editor_load(res,tmp,x,y,w,h);
	res->pythonjs=mode;
	i += taille ;
	return res;
      }
      pos = tmp.find("Editeur");
      if (pos>0 && pos<tmps){ 
	Editeur * res=0;
	next_line_nonl(s,L,line,i);
	// read size
	int taille,mode=python_compat(contextptr); string filename;
	read_size_mode(line,taille,mode,filename);
	string tmp=localize(s.substr(i,taille),language(contextptr));
	editeur_load(res,tmp,x,y,w,h);
	res->editor->pythonjs=mode; 
	if (filename.size()){
	  string * fname=new string(filename); // will be lost
	  res->output->value(fname->c_str());
	  res->editor->set_changed();//label(fname->c_str());
	}
	i += taille ;
	return res;
      }
      pos= tmp.find("Graph2d");
      if (pos<=0 || pos>=tmps)
	pos= tmp.find("Graphic");
      if (pos>0 && pos<tmps){ 
	Graph2d * res = new Graph2d(x,y,w,h,0);
	res->labelfont(police);
	graphic_load(res,s,L,line,i);
	return res;
      }
      pos= tmp.find("Geo2d");
      if (pos<=0 || pos>=tmps)
	pos= tmp.find("Geometry");
      if (pos>0 && pos<tmps){ 
	Geo2d * res = new Geo2d(x,y,w,h,0);
	res->labelfont(police);
	graphic_load(res,s,L,line,i);
	return res;
      }
#if 1 // def HAVE_LIBFLTK_GL
      pos= tmp.find("Graph3d");
      if (pos>0 && pos<tmps){ 
	Graph3d * res = new Graph3d(x,y,w,h,"",0);
	res->opengl=true;
	res->labelfont(police);
	graphic_load(res,s,L,line,i);
	res->autoscale(false);
	return res;
      }
      pos= tmp.find("Geo3d");
      if (pos>0 && pos<tmps){ 
	Geo3d * res = new Geo3d(x,y,w,h,0);
	res->labelfont(police);
	graphic_load(res,s,L,line,i);
	return res;
      }
#endif
      pos=tmp.find("Multiline_Input_tab");
      if (pos>0 && pos<tmps){
	Multiline_Input_tab * res = new Multiline_Input_tab(x,y,w,h);
	res->when(FL_WHEN_ENTER_KEY|FL_WHEN_NOT_CHANGED);
	res->callback(xcas::History_Pack_cb_eval,0);
	res->textcolor(Xcas_input_color);
	res->labelfont(police);
	res->textfont(police);
	next_line_nonl(s,L,line,i);
	line=localize(line,language(contextptr));
	res->value(line.c_str());
	res->set_changed();
	return res;
      }
      pos=tmp.find("Input");
      if (pos>0 && pos<tmps){
	Comment_Multiline_Input * res = new Comment_Multiline_Input(x,y,w,h);
	next_line_nonl(s,L,line,i);
	res->value(line.c_str());
	res->labelfont(police);
	res->textfont(police);
	res->callback((Fl_Callback*)Comment_cb_eval);
	res->when(FL_WHEN_ENTER_KEY|FL_WHEN_NOT_CHANGED);
	res->set_changed();
	res->textcolor(Xcas_comment_color);
	return res;
      }
      pos=tmp.find("Gen_Output");
      if (pos>0 && pos<tmps){
	Gen_Output * res = new Gen_Output(x,y,w,h);
	res->textcolor(FL_BLACK);
	res->labelfont(police);
	res->textfont(police);
	next_line_nonl(s,L,line,i);
	if (line.size()>=65536)
	  res->value("Object_too_large");
	else
	  res->value(line.c_str());
	return res;
      }
      pos=tmp.find("Output");
      if (pos>0 && pos<tmps){
	Log_Output * res = new Log_Output(x,y,w,h0);
	res->textcolor(Xcas_log_color);
	res->labelfont(police);
	res->textfont(police);
	next_line_nonl(s,L,line,i);
	res->value(line.c_str());
	return res;
      }
      pos=tmp.find("Gen_Value_Slider");
      if (pos>0 && pos<tmps){
	next_line_nonl(s,L,line,i);
#ifdef HAVE_SSTREAM
    istringstream is(line);
#else
    istrstream is(line.c_str());
#endif
	double m,M,val,step=0.1;
	int pos;
	string name;
	// is >> name ; // this is not the name but //, name is read below
	try {
	  is >> pos >> m >> M >> val >> name >> step;
	} catch(std::runtime_error & e) { 
	}
	Gen_Value_Slider * res=new Gen_Value_Slider(x,y,w-3*lsize,h,pos,m,M,(M-m)/100.,name);
	res->value(val);
	res->step(step);
	res->label(name.c_str());
	res->labelfont(police);
	res->labelsize(lsize);
	return res;
      }
    } // end if line begins with // fltk 
    else
      i=i_orig; // restore original position
    return 0;
  }

  const char * Xcas_pack_select(const xcas::History_Pack * pack,int sel_begin,int sel_end) {
    int n=pack->children();
    static std::string s;
    s="";
    if (n){
      int s1=sel_begin,s2=sel_end;
      if (s1>s2){ s1=sel_end; s2=sel_begin; }
      if (s2>=n) s2=n-1;
      if (s1<=0) s1=0;
      for (int i=s1;i<=s2;i++){
	s += widget_sprint(pack->child(i),pack->select_ans);
	if (i<s2)
	  s += ",\n";
      }
    }
    return s.c_str();
  }

  int Xcas_pack_insert(xcas::History_Pack * pack,const char * chaine,int length,int before_position) {
    if (!pack)
      return 0;
    Fl_Group::current(pack);
    context * contextptr = pack->contextptr;
    int n=pack->children();
    if (!n)
      before_position=n;
    else
      if (before_position<0 || before_position>=n) before_position=n-1;
    int l0=strlen(chaine);
    if (length>l0)
      l0=length;
    string s;
    for (int i0=0;i0<l0;i0++){
      // if (chaine[i0]!=13) 
	s += chaine[i0];
    }
    // cerr << s << '\n';
    int L=s.size(),i=0;
    // Check for an HTML link
    if (L>8 && (s.substr(0,6)=="http:/" || s.substr(0,7)=="https:/" || s.substr(0,6)=="file:/") ){
      // find # position, then create normal line for +, slider for *
      int pos=s.find('#');
      if (pos>0 && pos<L){
	int police=pack->labelfont(),lsize=pack->labelsize(),y=before_position;
	bool finished=false;
	while (!finished){
	  int nextpos=s.find('&',pos+1);
	  if (nextpos > L){
	    nextpos=L;
	    finished=true;
	  }
	  if (nextpos<pos+2)
	    break;
	  string txt=s.substr(pos+2,nextpos-pos-2);
	  txt=html_filter(txt);
	  if (s[pos+1]=='*'){
	    gen g(txt,contextptr);
	    if (g.type==_VECT && g._VECTptr->size()>=5){
	      txt="assume("+g[0].print(contextptr)+"=["+g[1].print(contextptr)+","+g[2].print(contextptr)+","+g[3].print(contextptr)+","+g[4].print(contextptr)+"])";
	    }
	  }
	  else {
	    if (s[pos+1]!='+'){
	      pos=nextpos;
	      continue;
	    }
	    if (s.size()>pos+3 && s[pos+2]=='/' && s[pos+3]=='/'){
	      txt=replace(txt,'\n',char(0x7f)); // should count \n and ajust size
	      txt="// fltk 7Fl_Tile 14 68 845 25 18 0\n[\n// fltk N4xcas23Comment_Multiline_InputE 14 68 845 24 18 0\n"+txt.substr(2,txt.size()-2)+"\n,\n// fltk N4xcas10Log_OutputE 14 93 845 1 18 0\n\n]";
	    }
	    else {
	      int pos=txt.find('\n');
	      if (pos>0 && pos<txt.size()){
		txt="// fltk 7Fl_Tile 14 68 845 254 18 0\n[\n// fltk N4xcas16Xcas_Text_EditorE 14 68 845 253 18 0\n"+print_INT_(txt.size())+" ,\n"+txt+",\n// fltk N4xcas10Log_OutputE 14 321 845 1 18 0\n\n]";
	      }
	    }
	  }
	  Xcas_pack_insert(pack,txt.c_str(),txt.size(),y);
	  ++y;
	  pos=nextpos;
	}
	pack->redraw();
	// next_line(s,L,line,i);
	return 1;	
      }
    }

    Fl_Widget * o;
    for (;i<L;++before_position){
      //cout << "i=" << i << ", L=" << L << ", s=" << s.substr(i,10) << "\n";
      if (i+7<L && s.substr(i,7)=="// fltk"){
	if ( (o=widget_load(s,L,i,contextptr,pack->w()-pack->_printlevel_w) )){
	  if (Fl_Group * g=dynamic_cast<Fl_Group *>(o))
	    pack->add_entry(before_position,g);
	  else
	    pack->add_entry(before_position,o);
	  if (i<L-1 && s[i]==',' && s[i+1]=='\n')
	    i += 2;
	  continue;
	}
      } // end if (i+7<L)
      if (i+11<L && s.substr(i,10)=="// context"){
#ifdef EMCC2
        break; // session is truncated...
#endif
	i+=11;
	int nchar=0;
	while (i<L && s[i]>='0' && s[i]<='9'){
	  nchar *= 10;
	  nchar += s[i]-'0';
	  ++i;
	}
	if (nchar<=L-i-1){
	  string tmps=s.substr(i+1,nchar);
	  gen replace;
	  if (!recovery_mode)
	    unarchive_session_string(tmps,-1,replace,contextptr);
	  i+=nchar+2;
	  continue;
	}
      }
      // Now add line in a multiline_input_tab entry
      string line;
      next_line_nonl(s,L,line,i);
      int ls=line.size();
      if (ls>2 && line[ls-1]==';' && line[ls-2]==';')
	line = line.substr(0,ls-1);
      if (ls>2 && line[0]=='/' && line[1]=='/')
	line = "/*"+line.substr(1,ls-1)+"*/";
      if (ls>4 && line[0]=='/' && line[1]=='*' && line[ls-1]=='/' && line[ls-2]=='*'){
	line=line.substr(2,ls-4);
	Comment_Multiline_Input * w = dynamic_cast<Comment_Multiline_Input *>(new_comment_input(max(pack->w()-pack->_printlevel_w,1),pack->labelsize()+10));
	if (w){
	  w->value(line.c_str());
	  w->set_changed();
	  pack->add_entry(before_position,w);
	}
      }
      else {
#if 0
	Multiline_Input_tab * w = dynamic_cast<Multiline_Input_tab *>(new_question_multiline_input(max(pack->w()-pack->_printlevel_w,1),pack->labelsize()+10));
#else 
	// requires call to update() to be commented in History_Pack::handle(int event) 
	// for FL_PASTE event
	Xcas_Text_Editor * w = dynamic_cast<Xcas_Text_Editor *>(new_question_editor(max(pack->w()-pack->_printlevel_w,1),pack->labelsize()+14));
#endif
	if (w){
	  w->value(line.c_str());
	  w->set_changed();
	  pack->add_entry(before_position,(Fl_Widget*) w);
	  w->resize_nl_before(1);
	}
      }
    } // end for
    change_group_fontsize(pack,pack->labelsize());
    set_context(pack,contextptr);
    parent_redraw(pack);
    return i;
  }

  giac::gen in_Xcas_fltk_interactive(const giac::gen & g,const giac::context * contextptr){
    if (is_zero(g,contextptr))
      return Xcas_DispG?Xcas_DispG->plot_instructions:undef;
    if (g.type==_SYMB){
      unary_function_ptr & u=g._SYMBptr->sommet;
      gen f=g._SYMBptr->feuille;
      if (u==at_pnt){
	//	Xcas_DispG_Window->show();
	// Xcas_Main_Window->show();
	redraw_turtle=1;
	if (Xcas_DispG && f.subtype!=_LOGO__VECT) 
	  Xcas_DispG->add(g);
	return 1;
      }
      if (u==at_equal){
	if (Xcas_DispG) Xcas_DispG->add(g);
	return 1;
      }
      if (u==at_erase){
	// Xcas_DispG_Window->show();
	// Xcas_Main_Window->show();
	if (Xcas_DispG) Xcas_DispG->clear();
	return 1;
      }
      if (u==at_Pictsize){
	if (Xcas_DispG){
	  // plot_instructionsw=Xcas_DispG->w();
	  // plot_instructionsh=Xcas_DispG->h();
	  gnuplot_xmax=Xcas_DispG->window_xmax;
	  gnuplot_xmin=Xcas_DispG->window_xmin;
	  gnuplot_ymax=Xcas_DispG->window_ymax;
	  gnuplot_ymin=Xcas_DispG->window_ymin;
	  return makevecteur(Xcas_DispG->w(),Xcas_DispG->h());
	}
	return makevecteur(0,0);
      }
      if (u==at_RclPic && f.type==_VECT){
	// Xcas_DispG_Window->show();
	// Xcas_Main_Window->show();
	if (Xcas_DispG) Xcas_DispG->add(*f._VECTptr);
	return 1;
      }
      if (u==at_RplcPic && f.type==_VECT){
	// Xcas_DispG_Window->show();
	// Xcas_Main_Window->show();
	if (Xcas_DispG){
	  Xcas_DispG->clear();
	  Xcas_DispG->add(*f._VECTptr);
	}
	return 1;
      }
      if (u==at_Pause){
	if (f.type==_STRNG)
	  xcas_paused=*f._STRNGptr;
	else
	  xcas_paused="Paused";
	for (int i=0;i<360000;++i){
	  if (xcas_paused.empty())
	    break;
	  usleep(10000);
	}
	return 1;
      }
      if (u==at_ClrIO){
	if (Xcas_PrintG) 
	  Xcas_PrintG->set_data(string2gen("",false));
	* logptr(contextptr) << "";
	return 1;
      }
      if (u==at_DispG){
	if (Xcas_DispG_Window) show_xcas_dispg=2;
#ifndef WIN32
	if (Xcas_Main_Window) Xcas_Main_Window->show();
#endif
	return 1;
      }
      if (u==at_DispHome){
	if (Xcas_DispG_Window) show_xcas_dispg=1;
	return 1;
      }
      /*
      if (u==at_Output){
	user_screen_io_x=f[0].val;
	user_screen_io_y=f[1].val;
	f=f[2];
      }
      if (u==at_Output || u==at_print){
	gen tmp=Eqw_compute_size(f,Eqw_history->attr,Eqw_history->w());
	eqwdata v=Eqw_total_size(tmp);
	if (Eqw_history->data.type!=_VECT && Eqw_history->data.type!=_EQW){
	  Eqw_history->data=tmp;
	  user_screen_io_y=v.y; // top of next writing
	}
	else {
	  eqwdata w=Eqw_total_size(Eqw_history->data);
	  int h=w.dy,y=w.y;
	  tmp=Eqw_translate(tmp,user_screen_io_x,user_screen_io_y-v.y-v.dy);
	  v=Eqw_total_size(tmp);
	  Eqw_vertical_adjust(v.dy,v.y,h,y);
	  user_screen_io_y = y;
	  user_screen_io_x = 0;
	  vecteur res;
	  if (Eqw_history->data.type==_EQW)
	    res.push_back(Eqw_history->data);
	  else
	    res=vecteur(Eqw_history->data._VECTptr->begin(),Eqw_history->data._VECTptr->end()-1);
	  res.push_back(tmp);
	  res.push_back(eqwdata(max(w.dx,v.dx),h,0,y,Eqw_history->attr,at_makevector,0));
	  gen gg=gen(res,_EQW__VECT);
	  Eqw_history->data=gg;
	}
	Eqw_history->setscroll();
	return f;
      }
      */
    }
    return 1;
  }
  
  giac::gen Xcas_fltk_interactive(const giac::gen & g,GIAC_CONTEXT){
#ifdef HAVE_LIBPTHREAD
    // cerr << "xcas lock" << g << '\n';
    int locked=pthread_mutex_trylock(&interactive_mutex);
    if (locked){
      usleep(100); // was usleep(1000)
      locked=pthread_mutex_trylock(&interactive_mutex);
      if (locked){
	// cerr << "locked " << g << '\n' ;
	return 0;
      }
    }
#endif
    if (block_signal){
      cerr << "blocked " << g << '\n';
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&interactive_mutex);
#endif
      return zero;
    }
    gen res=in_Xcas_fltk_interactive(g,contextptr); 
    // FIXME change interactive for context, like input
#ifdef HAVE_LIBPTHREAD
    // cerr << "xcas unlock" << '\n';
    pthread_mutex_unlock(&interactive_mutex);
#endif
    return res;
  }
  
  giac::gen in_Xcas_fltk_getKey(const giac::gen & g,giac::context * contextptr){
    int ch=Fl::event_key();
    if (Fl::event_key(ch))
      return ch;
    else
      return -ch;
  }

  giac::gen Xcas_fltk_getKey(const giac::gen & g,GIAC_CONTEXT){
    if (block_signal){
      return zero;
    }
    gen res;
    if (is_minus_one(g)){
      Fl::lock();
      res=in_Xcas_fltk_getKey(g,0); 
      Fl::unlock();
    }
    else
      res=Xcas_fltk_input(makevecteur(at_getKey,g),contextptr);
    // FIXME change interactive for context, like input
    if (is_inf(res))
      setsizeerr();
    return res;
  }
  
  vector <Fl_Browser *> vbrowser;
  vector<int> gbrowser;
  void cb_plotfltk_browser(Fl_Browser * b,void *){
    if (b->value()){
      vector <Fl_Browser *>::const_iterator it=vbrowser.begin(),itend=vbrowser.end();
      for (;it!=itend;++it){
	if (*it==b)
	  break;
      }
      if (it!=itend)
	gbrowser[it-vbrowser.begin()]=b->value();
    }
  }

  static int nlines(const string & s){
    int res=1;
    int ns=s.size()-1;
    for (int i=0;i<ns;++i){
      if (s[i]=='\n') ++res;
    }
    return res;
  }

  Fl_Window * getkeywin=0;
  class Fl_Key :public Fl_Input {
  public:
    int key;
    Fl_Key(int x,int y,int w,int h):Fl_Input(x,y,w,h){key=-1;}
    virtual int handle(int event){
      if (event==FL_KEYBOARD){
	int k=Fl::event_key();
	switch (k){
	case FL_Right:
	  key=3;
	  break;
	case FL_Left:
	  key=0;
	  break;
	case FL_Up:
	  key=1;
	  break;
	case FL_Down:
	  key=2;
	  break;
	}
	if (key>=0)
	  return 1;
      }
      return Fl_Input::handle(event);
    }    
  };
  
  // Given a vector v describing an input form, return
  gen makeform(const vecteur & v0,GIAC_CONTEXT) {
    vecteur v;
    aplatir(v0,v);
    if (v.size()==1 && v.front().is_symb_of_sommet(at_output)){
      fl_message("%s",eval(v.front()._SYMBptr->feuille,contextptr).print(contextptr).c_str());
      return plus_one;
    }
    if (v.size()==1 && v.front().type==_STRNG){
      v.push_back(identificateur("_input_"));
      // CERR << v << '\n';
    }
    if (!v.empty() && v.front()==at_getKey){
      Fl_Widget * foc=Fl::focus();
      static Fl_Button * getkeybut = 0;
      static Fl_Button * getkeyleft = 0;
      static Fl_Button * getkeyright = 0;
      static Fl_Button * getkeyup = 0;
      static Fl_Button * getkeydown = 0;
      static Fl_Button * getkeyesc = 0;
      static Fl_Button * getkeyok = 0;
      static Fl_Key * getkeyin = 0;
      static Fl_Multiline_Output * getkeyout = 0;
      static Graph2d * getkeyscreen = 0 ;
      if (!getkeywin){
	Fl_Group::current(0);
	getkeywin=new Fl_Window(50,50,460,360);
	getkeywin->label(gettext("Press a key"));
	getkeyscreen=new Graph2d(0,120,460,240);
	getkeyscreen->show_axes=0;
	// getkeyscreen->legende_size=0;
	getkeyout= new Fl_Multiline_Output(2,64,456,56);
	getkeybut=new Fl_Button(2,2,96,20);
	getkeybut->label(gettext("Cancel"));
	getkeybut->shortcut("^[");
	getkeyesc=new Fl_Button(2,34,36,20);
	getkeyesc->label("esc");
	getkeyleft=new Fl_Button(42,34,36,20);
	getkeyleft->label("◀");
	getkeyleft->shortcut(0xff51);
	getkeyright=new Fl_Button(122,34,36,20);
	getkeyright->label("▶");
	getkeyright->shortcut(0xff53);
	getkeyup=new Fl_Button(82,24,36,20);
	getkeyup->label("▲");
	getkeyup->shortcut(0xff52);
	getkeydown=new Fl_Button(82,44,36,20);
	getkeydown->label("▼");
	getkeydown->shortcut(0xff54);
	getkeyok=new Fl_Button(162,34,36,20);
	getkeyok->label("OK");
	getkeyesc->shortcut(0xff1b);
	getkeyok->shortcut(0xff0d);
	getkeyin = new Fl_Key(102,2,96,20);
	getkeyin->when(FL_WHEN_CHANGED);
	getkeywin->end();
	getkeywin->resizable(getkeyscreen); // (getkeywin);
      }
      getkeyin->key=-1;
      getkeyscreen->clear();
      vecteur V=get_pixel_v();
      getkeyscreen->add(V);
      int I=320,J=240;
      adjust_pixels_dim(V,I,J);
      I=giacmin(I,giac::screen_w);
      J=giacmin(J,giac::screen_h);
      getkeywin->resize(getkeywin->x(),getkeywin->y(),I+140,J+120);
      getkeyout->resize(2,64,456,56);
      getkeyscreen->resize(0,120,I+140,J);
      //getkeyscreen->resize_mouse_param_group(140);
      getkeyscreen->mouse_param_group->redraw();
      //getkeyscreen->redraw();
      string msg; double delay=-1;
      int vs=v.size();
      if (vs==2 && v[1].type==_DOUBLE_){
	delay=v[1]._DOUBLE_val;
	if (delay<0)
	  delay=0;
      }
      for (int i=1;i<vs;++i){
	if (v[i].type==_STRNG)
	  msg += *v[i]._STRNGptr;
	else
	  msg += v[i].print(contextptr);
	if (i==vs-1)
	  break;
	msg += '\n';
      }
      if (delay==-1)
	msg += gettext("Press a key\n");
      getkeyout->value(msg.c_str());
      getkeyin->value("");
      getkeywin->show();
      getkeywin->set_modal();
      Fl::focus(getkeyin);
      if (Xcas_Main_Window){
	Xcas_Main_Window->redraw();
      }
      Fl::flush();
      gen res=undef;
      for (int n=0;;n++){
	if (getkeyin->key>=0){
	  res=getkeyin->key;
	  break;
	}
	Fl_Widget *o = Fl::readqueue();
	if (!o){
	  if (delay>=0){
	    Fl::wait(0.001);
	    usleep(1000);
	    if (n>delay*500){
	      res=0;
	      break;
	    }
	    else
	      continue;
	  }
	  Fl::wait();
	}
	if (o==getkeybut)
	  res=unsigned_inf;
	if (o==getkeybut || o==getkeywin){
	  break;
	}
	if (o==getkeyesc){
	  res=5;
	  break;
	}
	if (o==getkeyok){
	  res=4;
	  break;
	}
	if (o==getkeyleft){
	  res=0;
	  break;
	}
	if (o==getkeyright){
	  res=3;
	  break;
	}
	if (o==getkeyup){
	  res=1;
	  break;
	}
	if (o==getkeydown){
	  res=2;
	  break;
	}
	if (o==getkeyin){
	  int l=strlen(getkeyin->value());
	  if (l){
	    res=getkeyin->value()[l-1];
	    break;
	  }
	}
      }
      // getkeywin->hide();
      Fl::focus(foc);
      return res;
    }
    int initw,inith;
    if (Xcas_input_focus && Xcas_input_focus->window()){
      initw=(Xcas_input_focus->window()->w()*3)/4;
      inith=(Xcas_input_focus->window()->h()*3)/4;
    }
    else {
      initw = 240;
      inith = 320;
    } 
    Fl_Group::current(0);
    static Fl_Window * Plotfltk_w = new Fl_Window(initw,inith);
    int r;
    vecteur res;
    Fl_Window * w = Plotfltk_w ;
    w->end();
    int taillew=w->w(),tailleh=w->h();
    if (taillew<100 || tailleh<100){
      if (taillew<100)
	taillew=100;
      if (tailleh<100)
	tailleh=100;
      w->resize(w->x(),w->y(),taillew,tailleh);
    }
    Fl_Group::current(w);
    static Fl_Button * button0 = new Fl_Button(2+(2*taillew)/3, 0, taillew/4, 20);
    button0->shortcut("^[");
    button0->resize(2+(2*taillew)/3, 0, taillew/4, 20);
    button0->label(gettext("Cancel"));
    static Fl_Button * button1 = new Fl_Return_Button(2+taillew/3, 0, taillew/4, 20);
    button1->label(gettext("OK"));
    button1->resize(2+taillew/3, 0, taillew/4, 20);
    static Fl_Button * button2 = new Fl_Button(2, 0, taillew/4, 20);
    button2->label(gettext("STOP"));
    button2->resize(2, 0, taillew/4, 20);
    // Now parse v
    int current_y=22;
    vector<Fl_Input *> vinput;
    vector<Fl_Output *> voutput;
    vector<string *> labels;
    /*
    vector <Fl_Menu_Button *> vpopup;
    vector < vector<Fl_Menu_Item * > > vpopupitem;
    */
    const_iterateur it=v.begin(),itend=v.end();
    bool focused=false;
    for (;it!=itend;++it){
      if (it->type==_IDNT || it->is_symb_of_sommet(at_of) || it->is_symb_of_sommet(at_at)){
	Fl_Input * o;
	string * l=new string (it->print(contextptr));
	labels.push_back(l);
	int taille=4+int(fl_width(l->c_str()));
	if (taille>taillew/2)
	  taille=taillew/2;
	int yadd=6+nlines(*l)*14;
	o=new Fl_Input(taille,current_y,taillew-taille,yadd,l->c_str());
	vinput.push_back(o);
	if (!focused){
	  focused=true;
	  Fl::focus(o);
	}
	current_y += yadd;
	continue;
      }
      if ( (it->type==_STRNG) && it+1!=itend && ((it+1)->type==_IDNT || (it+1)->is_symb_of_sommet(at_at) || (it+1)->is_symb_of_sommet(at_of)) ){
	Fl_Input * o;
	string * l=new string (*it->_STRNGptr);
	labels.push_back(l);
	int yadd=6+nlines(*l)*14;
	int taille=4+int(fl_width(l->c_str()));
	if (taille>taillew/2)
	  taille=taillew/2;
	o=new Fl_Input(taille,current_y,taillew-taille,yadd,l->c_str());
	++it;
	vinput.push_back(o);
	if (!focused){
	  focused=true;
	  Fl::focus(o);
	}
	current_y += yadd;
	continue;
      }
      if (it->type!=_SYMB)
	continue;
      unary_function_ptr & u=it->_SYMBptr->sommet;
      gen g=it->_SYMBptr->feuille;
      if ( (u==at_click) || (u==at_Request) ) {
	vecteur vg(gen2vecteur(g));
	if (vg.empty())
	  continue;
	Fl_Input * o;
	gen tmps=vg[0].eval(eval_level(contextptr),contextptr);
	string * l=new string (tmps.type==_STRNG?*tmps._STRNGptr:tmps.print(contextptr));
	labels.push_back(l);
	int taille=4+int(fl_width(l->c_str()));
	if (taille>taillew/2)
	  taille=taillew/2;
	o=new Fl_Input(taille,current_y,taillew-taille,20,l->c_str());
	if (vg.size()>1&& u!=at_Request)
	  o->value(vg[1].eval(eval_level(contextptr),contextptr).print(contextptr).c_str());
	vinput.push_back(o);
	if (!focused){
	  focused=true;
	  Fl::focus(o);
	}
	current_y +=20;
	continue;
      }
      if ( (u==at_output) || (u==at_Text) || (u==at_Title) ){
	g=g.eval(eval_level(contextptr),contextptr);
	Fl_Output * o;
	if (u==at_Title){
	  string * l=new string (g.type==_STRNG?*g._STRNGptr:g.print(contextptr));
	  labels.push_back(l);
	  o=new Fl_Output(taillew/2-40,current_y,0,20,l->c_str());
	  o->value("");
	}
	else {
	  o=new Fl_Output(40,current_y,taillew-60,20,"");
	  o->value((g.type==_STRNG?*g._STRNGptr:g.print(contextptr)).c_str());
	}
	voutput.push_back(o);
	current_y += 20;
	continue;
      }
      /*
      if (u==at_popup){
	vecteur vg(gen2vecteur(g));
	if (vg.empty())
	  continue;
	Fl_Menu_Button * o=new Fl_Menu_Button(10,current_y,100,20,vg[0].print(contextptr).c_str());
	vpopup.push_back(o);
	vector<Fl_Menu_Item *> tmpmenu;
	const iterateur tmpit=vg.begin()+1,tmpend=vg.end();
	int y=current_y;
	for (;tmpit!=tmpend;++tmpit){
	  Fl_Menu_Item * o=new Fl_Menu_Item(10,y,100,15,tmpit->print(contextptr).c_str());
	  tmpmenu.push_back(o);
	  y +=15;
	}
	vpopupitem.push_back(tmpmenu);
	o->end();
	current_y +=20;
	continue;
      }
      */
      if ( (u==at_choosebox) || (u==at_DropDown) || (u==at_Popup) ){
	vecteur vg(gen2vecteur(g));
	if (vg.size()==2)
	  vg.insert(vg.begin(),string2gen("",false));
	if (vg.size()!=3)
	  continue;
	int hs=64;
	if (vg[1].type==_VECT)
	  hs=vg[1]._VECTptr->size()*16+5;
	hs=min(hs,160);
	Fl_Browser * o =new Fl_Browser(50, current_y+16, 180, hs);
	current_y += hs+16;
        o->type(2);
	string * l=new string (vg[0].type==_STRNG?*vg[0]._STRNGptr:vg[0].print(contextptr));
	labels.push_back(l);
        o->label(l->c_str());
	o->align(FL_ALIGN_TOP);
        o->callback((Fl_Callback*)cb_plotfltk_browser);
	vbrowser.push_back(o);
	gbrowser.push_back(0);
	vecteur wg(gen2vecteur(vg[1]));
	const_iterateur jt=wg.begin(),jtend=wg.end();
	for (;jt!=jtend;++jt){
	  o->add(jt->print(contextptr).c_str());
	}
	gen g=protecteval(vg[2],eval_level(contextptr),contextptr);
	if ( (g.type==_INT_) && (g.val>0) && (g.val<=int(wg.size())) )
	  o->value(g.val);
	continue;
      }
    }
    Xcas_Main_Window->redraw();    
    w->Fl_Widget::resize(w->x(),w->y(),taillew,current_y);
    w->resizable(w);
    w->set_modal();
    w->show();
    for (;;) {
      Fl_Widget *o = Fl::readqueue();
      if (!o) Fl::wait();
      else {
	if (o == button0) {r = 0; break;}
	if (o == button1) {r = 1; break;}
	if (o == button2) {r = 2; break;}
	if (o == w) { r=0; break; }
      }
    }
    w->hide();
    bool request=false;
    gen gtmp("ok",contextptr);
    res.push_back(symb_sto(r,gtmp));
    // store results
    it=v.begin();
    int ibrowser=0;
    vector<Fl_Input *>::const_iterator vinput_it=vinput.begin();
    gen resadd,tmp;
    for (;it!=itend;++it){
      if (it->type==_IDNT || it->is_symb_of_sommet(at_at) || it->is_symb_of_sommet(at_of)){
	if (it+1!=itend && *(it+1)==1)
	  resadd=string2gen((*vinput_it)->value(),false);
	else {
	  try {
	    resadd=gen((*vinput_it)->value(),contextptr);
	  }
	  catch (std::runtime_error & e){
	    resadd=string2gen(e.what(),false);
	  }
	}
	tmp=*it;
	res.push_back(symb_sto(resadd,tmp));
	++vinput_it;
      }
      if (it->type!=_SYMB)
	continue;
      unary_function_ptr & u=it->_SYMBptr->sommet;
      gen & g=it->_SYMBptr->feuille;
      if ( (u==at_click) || (u==at_Request) ){
	vecteur vg(gen2vecteur(g));
	try {
	  if (vg.size()>3|| u==at_Request){
	    request=true;
	    resadd=string2gen((*vinput_it)->value(),false);
	  }
	  else
	    resadd=gen((*vinput_it)->value(),contextptr);
	}
	catch (std::runtime_error & e){
	  resadd=string2gen(e.what(),false);
	}
	if (u==at_Request && vg.size()>1){
	  res.push_back(symb_sto(resadd,vg[1]));
	  request=true;
	}
	else {
	  if (vg.size()>2)
	    res.push_back(symb_sto(resadd,vg[2]));
	}
	++vinput_it;
	continue;
      }
      if ( (u==at_choosebox) || (u==at_DropDown) || (u==at_Popup) ){
	vecteur vg(gen2vecteur(g));
	if (vg.size()==2)
	  vg.insert(vg.begin(),zero);
	if (vg.size()!=3)
	  continue;
	res.push_back(symb_sto(gbrowser[ibrowser],vg[2]));
	++ibrowser;
	continue;
      }
    }
    // delete widgets
    { 
      vector<Fl_Input *>::const_iterator jt=vinput.begin(),jtend=vinput.end();
      for (;jt!=jtend;++jt){
	w->Fl_Group::remove(*jt);
	delete *jt;
      }
      vinput.clear();
    }
    { 
      vector<Fl_Output *>::const_iterator jt=voutput.begin(),jtend=voutput.end();
      for (;jt!=jtend;++jt){
	w->Fl_Group::remove(*jt);
	delete *jt;
      }
      voutput.clear();
    }
    { 
      vector<Fl_Browser *>::const_iterator jt=vbrowser.begin(),jtend=vbrowser.end();
      for (;jt!=jtend;++jt){
	w->Fl_Group::remove(*jt);
	delete *jt;
      }
      vbrowser.clear();
    }
    { 
      vector<string *>::const_iterator jt=labels.begin(),jtend=labels.end();
      for (;jt!=jtend;++jt){
	delete *jt;
      }
      labels.clear();
    }
    /*
    { 
      vector<Fl_Menu_Button *>::const_iterator jt=vpopup.begin(),jtend=vpopup.end();
      for (;jt!=jtend;++jt){
	w->Fl_Group::remove(*jt);
	delete *jt;
      }
    }
    { 
      vector< vector<Fl_Menu_Item *> >::const_iterator jt=vpopupitem.begin(),jtend=vpopupitem.end();
      for (;jt!=jtend;++jt){
	vector<Fl_Menu_Item *>::const_iterator jtt=jt->begin(),jttend=jt->end();
	for (;jtt!=jttend;+jtt)
	w->Fl_Group::remove(*jt);
	  delete *jtt;
      }
    }
    */
    // delete button1;
    // delete button0;
    // delete w;
    if (r==2)
      return 0;
    if (request)
      return res;
    if (r==0)
      return undef;
    else
      return res;
  }

  // FIXME: forms should work under win32!!
#if defined __APPLE__ || defined WIN32 
  giac::gen Xcas_fltk_input(const giac::gen & arg,const giac::context * contextptr){
    Fl::lock();
    if (Xcas_DispG){
      if (arg==at_getKey)
	Xcas_DispG->waiting_click_value=makevecteur(arg,gen(vecteur(0),_SEQ__VECT));
      else
	Xcas_DispG->waiting_click_value=arg;
    }
    Fl::unlock();
    thread_eval_status(3,contextptr);
    for (;;){
      if (thread_eval_status(contextptr)==1)
	break;
      usleep(10000);
    }
    Fl::lock();
    gen res=Xcas_DispG?Xcas_DispG->waiting_click_value:undef;
    Fl::unlock();
    return res.eval(eval_level(contextptr),contextptr);
    // return res;
  }

  gen Xcas_fltk_inputform(const gen & args,const giac::context * contextptr){
    return Xcas_fltk_input(args,contextptr);
  }

#else

  gen Xcas_fltk_inputform(const gen & args,const giac::context * contextptr){
    if (args.type==_VECT && args._VECTptr->empty())
      return Xcas_fltk_inputform(identificateur("tmp_input"),contextptr);
    vecteur v(inputform_pre_analysis(args,contextptr));
    Fl::lock();
    gen res=makeform(v,contextptr);
    Fl::unlock();
    return inputform_post_analysis(v,res,contextptr);
  }

  giac::gen Xcas_fltk_input(const giac::gen & arg,const giac::context * contextptr){
    vecteur v(inputform_pre_analysis(arg,contextptr));
    if (v.empty()){
      Fl::lock();
      if (Xcas_DispG_Window) show_xcas_dispg=2;
      if (Xcas_DispG_Cancel) Xcas_DispG_Cancel->show();
      if (Xcas_DispG) Xcas_DispG->waiting_click=true;
      Fl::unlock();
      for (;;) {
#ifdef HAVE_LIBPTHREAD
	usleep(10000);
#else
	Fl::wait();
#endif // HAVE_LIBPTHREAD
	Fl::lock();
	bool wait= (Xcas_DispG && Xcas_DispG_Window && Xcas_DispG_Window->visible())? Xcas_DispG->waiting_click : false;
	Fl::unlock();
	if (!wait)
	  break;
      }
      Fl::lock();
      if (Xcas_DispG) Xcas_DispG->waiting_click=false;
      if (Xcas_DispG_Cancel) Xcas_DispG_Cancel->hide();
      gen res=(Xcas_DispG && Xcas_DispG_Window && Xcas_DispG_Window->visible())?Xcas_DispG->waiting_click_value:undef;
      Fl::unlock();
      return res;
    }
    else {
      Fl::lock();
      gen res=makeform(v,contextptr);
      Fl::unlock();
      return inputform_post_analysis(v,res,contextptr);
    }
  }

#endif // WIN32

#endif // HAVE_LIBFLTK

  // functions for icas or C-library usage
  int read_file(const giac::gen & g){
    if (g.is_symb_of_sommet(giac::at_read))
      return 1;
    if (g.is_symb_of_sommet(giac::at_geo2d))
      return 2;
    if (g.is_symb_of_sommet(giac::at_geo3d))
      return 3;
    if (g.is_symb_of_sommet(giac::at_spreadsheet))
      return 4;
    return 0;
  }

  void icas_eval_callback(const giac::gen & evaled_g,void * param){
    giac::gen * resptr=(giac::gen *) param;
    // cerr << "icas_eval_callback " << evaled_g << '\n';
    *resptr=evaled_g;
  }
  
  // eval g to gg, if g is a read command, replace g by the file content
  // and set reading_file to true
  void icas_eval(giac::gen & g,giac::gen & gg,int & reading_file,std::string &filename,giac::context * contextptr){
    if (debug_infolevel)
      CERR << CLOCK() << " icas_eval " << g << '\n';
    try {
      reading_file=read_file(g);
      if (g.type==_SYMB && g._SYMBptr->feuille.type==giac::_STRNG)
	filename=reading_file?*g._SYMBptr->feuille._STRNGptr:"";
      if (g.type==_SYMB && reading_file){
	if (g._SYMBptr->feuille.type==giac::_VECT && !g._SYMBptr->feuille._VECTptr->empty()){
	  gg=_read(g._SYMBptr->feuille,contextptr);
	  return;
	}
	else {
	  if (g._SYMBptr->feuille.type!=giac::_STRNG)
	    g=g._SYMBptr->feuille;
	  else
	    g=quote_read(g._SYMBptr->feuille,contextptr);
	}
      }
      if (g.type==giac::_VECT && !filename.empty()){
	// patch to handle spreadsheet[ correctly
	FILE * inf=fopen(filename.c_str(),"r");
	if (inf){
	  char ch[13];
	  ch[12]=0;
	  if (fread(ch,1,12,inf)==12 && !strcmp(ch,"spreadsheet[")){
	    giac::makespreadsheetmatrice(*g._VECTptr,0);
	    g.subtype=giac::_SPREAD__VECT;
	  }
	  fclose(inf);
	}
      }
      giac::gen g1=giac::approx_mode(contextptr)?giac::symbolic(giac::at_evalf,g):g;
      giac::gen result;
      signal(SIGINT,ctrl_c_signal_handler);
#ifdef HAVE_LIBPTHREAD
      giac::make_thread(g1,eval_level(contextptr),icas_eval_callback,&result,contextptr);
      int status;
      while (1){
	// look at other threads
	int cs=context_list().size(),ci=0;
	for (;ci<cs;++ci){
	  context * cptr=context_list()[ci];
	  if (cptr!=contextptr)
	    status=check_thread(cptr);
	}
	// 0 finished, 2/3 debug/wait click
	status=giac::check_thread(contextptr);
#ifdef HAVE_LIBFLTK
	if (xcas::Xcas_Debug_Window && status<2)
	  xcas::Xcas_Debug_Window->hide();
#endif
	if (status<=0){
#ifdef HAVE_LIBFLTK
	  Fl::flush();
#endif
	  break;
	}
#ifdef HAVE_LIBFLTK
	if (status!=1)
	  xcas::Xcas_debugguer(status,contextptr);
#else
	// FIXME Debugguer without FLTK
	if (status!=1)
	  giac::thread_eval_status(1,contextptr);
#endif
	if (ctrl_c){
	  if (giac::is_context_busy(contextptr) && giac::kill_thread(contextptr)!=2)
	    giac::kill_thread(1,contextptr);
	  ctrl_c=false; interrupted=false;
	}
	else
	  usleep(1000);
      }
      gg=result;
#else
      gg=eval(g1,eval_level(contextptr),contextptr);
#endif
    }
    catch(std::runtime_error & e){
      if (!contextptr)
	giac::protection_level=0;
      gg=giac::string2gen(e.what(),false);
    }
  }

  int fltk_return_value=-1;
#ifdef HAVE_LIBFLTK
  static void cb_Close(Fl_Button * m , void*) {
    if (m->window())
      m->window()->hide();
    fltk_return_value=0;
  }
  
  static void cb_Cancel(Fl_Button * m , void*) {
    if (m->window())
      m->window()->hide();
    fltk_return_value=1;
  }
  
  static void cb_Kill(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Pack * hp=xcas::get_history_pack(xcas::Xcas_input_focus);
    try {
      giac::kill_thread(1,hp?hp->contextptr:context0);
    } catch (...){
    }
  }

  static void cb_Browser(Fl_Button * m , void*) {
    xcas::use_external_browser=!xcas::use_external_browser;
  }
  
  static void cb_save(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      hf->pack->save();
  }

  static void cb_save_as(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      hf->pack->save_as(0);
  }

  static void cb_insert(Fl_Button * m , void*) {
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      hf->pack->insert_before(hf->pack->_sel_begin);
  }

  static void cb_exec(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      hf->eval();
  }

  static void cb_add_entry(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Pack * hp=xcas::get_history_pack(xcas::Xcas_input_focus);
    if (hp)
      hp->add_entry(-1);
  }

  static void cb_add_program(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      xcas::History_cb_New_Program(hf,0);
  }

  static void cb_add_tableur(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      xcas::History_cb_New_Tableur(hf,0);
  }

  static void cb_add_figure(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      xcas::History_cb_New_Figure(hf,0);
  }

  static void cb_add_figure3d(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      xcas::History_cb_New_Figure3d(hf,0);
  }

  static void cb_add_comment(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      xcas::History_cb_New_Comment_Input(hf,0);
  }

  static void cb_add_logo(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      xcas::History_cb_New_Logo(hf,0);
  }

  static void cb_add_equation(Fl_Button * m , void*) {
    // FIXME user_data()
    History_Fold * hf=xcas::get_history_fold(xcas::Xcas_input_focus);
    if (hf)
      xcas::History_cb_New_Equation(hf,0);
  }

  static void cb_help_index(Fl_Menu_*, void*) {
    static std::string ans; 
    int remove,ii;
    Fl_Widget * w=xcas::Xcas_input_focus;
    if (
	(ii=xcas::handle_tab("",(*giac::vector_completions_ptr()),600,400,remove,ans)) ){ 
      if (ii==1)
	ans = ans +"()";
      Fl::e_text = (char * ) ans.c_str();
      Fl::e_length = ans.size();
      if (w){
	xcas::fl_handle(w);
	if (Fl_Input * in =dynamic_cast<Fl_Input *>(w)){
	  if (ii==1) in->position(in->position()-1);
	}
      }
    };
  }

  static void cb_help_find(Fl_Menu_*, void*) {
    xcas::help_fltk("");
  }

  static void cb_help_casinter(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"casinter/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"casinter/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  const giac::context * Xcas_get_context() {
    return xcas::get_context(Xcas_input_focus);
  }

  static void cb_help_CAS(Fl_Menu_*, void*) {
    const giac::context * contextptr=Xcas_get_context();
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"cascmd_"+giac::find_lang_prefix(giac::language(contextptr))+"index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"cascmd_"+giac::find_lang_prefix(giac::language(contextptr))+"index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_graphtheory(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command("doc/graphtheory-user_manual.pdf");
    else {
      if (xcas::Xcas_help_window){ //disabled because it crashes with builin browser
	fl_message("%s",("Open with your browser "+giac::giac_aide_dir()+"doc/graphtheory-user_manual.pdf").c_str());
	// xcas::Xcas_help_window->load((giac::giac_aide_dir()+"algo.html").c_str());
	// xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_algo(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"algo.html");
    else {
      if (xcas::Xcas_help_window){ //disabled because it crashes with builin browser
	fl_message("%s",("Open with your browser "+giac::giac_aide_dir()+doc_prefix+"algo.html").c_str());
	// xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"algo.html").c_str());
	// xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_algo_pdf(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"algo.pdf");
    else {
      if (xcas::Xcas_help_window){
	fl_message("%s",("Open "+giac::giac_aide_dir()+doc_prefix+"algo.pdf").c_str());
	// xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"algo.pdf").c_str());
	// xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_Geo(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"casgeo/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"casgeo/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_Prog(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"casrouge/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"casrouge/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_Tableur(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"cassim/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"cassim/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_Tortue(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"castor/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"castor/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_Exercices(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"casexo/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"casexo/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_Amusement(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(doc_prefix+"cascas/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+doc_prefix+"cascas/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  static void cb_help_PARI(Fl_Menu_*, void*) {
    if (xcas::use_external_browser)
      giac::system_browser_command(giac::giac_aide_dir()+"doc/pari/index.html");
    else {
      if (xcas::Xcas_help_window){
	xcas::Xcas_help_window->load((giac::giac_aide_dir()+"doc/pari/index.html").c_str());
	xcas::Xcas_help_window->show();
      }
    };
  }

  bool has_graph3d(Fl_Widget * widget){
    Fl_Group * g=dynamic_cast<Fl_Group *>(widget);
    if (!g)
      return false;
    int n=g->children();
    for (int i=0;i<n;++i){
      Fl_Widget * wid = g->child(i);
      if (Fl_Group * gr=dynamic_cast<Fl_Group * >(wid)){
	if (has_graph3d(gr))
	  return true;
      }
      if (Graph3d * gr3=dynamic_cast<Graph3d * >(wid))
	return true;
    }
    return false;
  }
#endif

  void quit_idle_function(void * widget){
    static int t=0;
    if (!widget){
      t=CLOCK();
      return;
    }
#ifdef HAVE_LIBFLTK
    Fl_Widget * w=(Fl_Widget *)(widget);
    if (has_graph3d(w) && (CLOCK()-t<1e5)) return;
    w->hide();
    fltk_return_value=0;
#endif
  }

#ifdef HAVE_LIBFLTK
  void Menu_Insert_ItemName(Fl_Widget * w , void*) {
    static std::string menu_buffer;
    if (xcas::fl_handle_lock)
      return ;
    Fl_Menu_ * m =dynamic_cast<Fl_Menu_ *>(w);
    if (!m)
      return ;
    menu_buffer = m->text();
    int pos=menu_buffer.find(':');
    if (pos>0 && pos<menu_buffer.size()) menu_buffer=menu_buffer.substr(0,pos);
    int pos2=menu_buffer.find(' ');
    Fl_Widget * f = xcas::Xcas_input_focus;
    if (!f) return;
    static std::string ans;
    if (pos2>=0 && pos2<menu_buffer.size()){
      ans=menu_buffer;
      Fl::focus(f);
      if (xcas::Xcas_Text_Editor * in =dynamic_cast<xcas::Xcas_Text_Editor *>(f)){
	in->buffer()->insert(in->insert_position(),ans.c_str());
	in->insert_position(in->insert_position()+ans.size());
	return;
      }
    }
    const giac::context * contextptr = xcas::get_context(f);
    giac::gen tmp(menu_buffer,contextptr);
    if (1)
      xcas::browser_help(tmp,giac::language(contextptr));
    else
      xcas::help_output(menu_buffer,giac::language(contextptr));
    if (tmp.type==giac::_FUNC){
      if (xcas::Equation * eqwptr = dynamic_cast<xcas::Equation *>(f)){
  	eqwptr->parse_desactivate();
  	if (eqwptr->output_equation)
  	  eqwptr->eval_function(tmp);
  	else 
  	  eqwptr->replace_selection(giac::symbolic(*tmp._FUNCptr,eqwptr->get_selection()));
  	return;
      }
      if (0) // !Xcas_automatic_completion_browser->value())
	menu_buffer += '(';
    }
    ans=menu_buffer;
    int remove;
    Fl_Widget * wid=f->window();
    if (!wid) wid=f;
    pos=menu_buffer.find('('); // detect a composite unit
    if (menu_buffer.size()>1 && menu_buffer!="%0" && !(pos>0 && pos<menu_buffer.size()) && 1 /* Xcas_automatic_completion_browser->value()*/){
      if (!xcas::handle_tab(menu_buffer,(*giac::vector_completions_ptr()),2*wid->w()/3,2*wid->h()/3,remove,ans))
	return;
    }
    Fl::focus(f);
    if (xcas::Xcas_Text_Editor * in =dynamic_cast<xcas::Xcas_Text_Editor *>(f)){
      in->buffer()->insert(in->insert_position(),ans.c_str());
      in->insert_position(in->insert_position()+ans.size());
      return;
    }
    if (Fl_Input * in = dynamic_cast<Fl_Input *>(f))
      in->insert(ans.c_str());
    else {
      Fl::e_text = (char * ) ans.c_str();
      Fl::e_length = ans.size();
      // Fl::e_keysym = '\n';
      xcas::fl_handle(f);
    }
  }

  Fl_Menu_Item icas_xcas_menu[] = {
    {"File", 0,  0, 0, 64, FL_NORMAL_LABEL, 0, 14, 0},
    {"Save", 0x80073,  (Fl_Callback*)cb_save, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Save as", 0,  (Fl_Callback*)cb_save_as, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Insert", 0,  (Fl_Callback*)cb_insert, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Quit", 0,  (Fl_Callback*)cb_Close, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {0,0,0,0,0,0,0,0,0},    
    {"Edit", 0,  0, 0, 64, FL_NORMAL_LABEL, 0, 14, 0},
    {"Execute worksheet", 0x4ffc6,  (Fl_Callback*)cb_exec, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New entry", 0x8006e,  (Fl_Callback*)cb_add_entry, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New program", 0x80070,  (Fl_Callback*)cb_add_program, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New comment", 0x8006e,  (Fl_Callback*)cb_add_comment, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New figure 2d", 0x80067,  (Fl_Callback*)cb_add_figure, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New figure 3d", 0x80068,  (Fl_Callback*)cb_add_figure3d, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New spreadsheet", 0x80074,  (Fl_Callback*)cb_add_tableur, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New expression", 0x80065,  (Fl_Callback*)cb_add_equation, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"New logo turtle", 0x8006c,  (Fl_Callback*)cb_add_logo, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {0,0,0,0,0,0,0,0,0},    
    {"Help", 0,  0, 0, 64, FL_NORMAL_LABEL, 0, 14, 0},
    {"Index", 0,  (Fl_Callback*)cb_help_index, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Find word in HTML help", 0xffc9,  (Fl_Callback*)cb_help_find, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Interface", 0,  (Fl_Callback*)cb_help_casinter, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Manuals", 0,  0, 0, 64, FL_NORMAL_LABEL, 0, 14, 0},
    {"CAS reference", 0,  (Fl_Callback*)cb_help_CAS, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Graph theory", 0,  (Fl_Callback*)cb_help_graphtheory, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Algorithmes (HTML)", 0,  (Fl_Callback*)cb_help_algo, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Algorithmes (PDF)", 0,  (Fl_Callback*)cb_help_algo_pdf, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Geometry", 0,  (Fl_Callback*)cb_help_Geo, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Programmation", 0,  (Fl_Callback*)cb_help_Prog, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Simulation", 0,  (Fl_Callback*)cb_help_Tableur, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Turtle", 0,  (Fl_Callback*)cb_help_Tortue, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Exercices", 0,  (Fl_Callback*)cb_help_Exercices, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"Amusement", 0,  (Fl_Callback*)cb_help_Amusement, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {"PARI-GP", 0,  (Fl_Callback*)cb_help_PARI, 0, 0, FL_NORMAL_LABEL, 0, 14, 0},
    {0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0},    
  }; 
#endif

  // open a FLTK window, that will be printed to filename when closed
  // return false if FLTK not avail 
  bool fltk_view(const giac::gen & g,giac::gen & ge,const std::string & filename,std::string & figure_filename,int file_type,const giac::context *contextptr){
#ifdef __APPLE__
    // return false;
#endif
#ifdef HAVE_LIBFLTK
#if !defined(__APPLE__) && !defined(WIN32)
    if (!getenv("DISPLAY"))
      return false;
#endif    // FIXME GIAC_CONTEXT
    // const giac::context * contextptr = get_context(Fl_Group::current());
    bool geometry=!figure_filename.empty();
    if (file_type==4 && figure_filename.empty()){
      figure_filename="table.tab";
      geometry=true;
    }
    Fl_Window * w=0;
    Fl_Return_Button * button0 = 0 ;
    Fl_Button * button1 =0,*button2=0;
    Fl_Check_Button * browser=0;
    Fl_Menu_Bar * menu=0;
    if (!w){
      int dx=800,dy=(geometry || file_type==5)?500:360;
      Fl_Group::current(0);
      w=new Fl_Window(dx,dy);
      button0 = new Fl_Return_Button(2,2,dx/3-4,20);
      button0->shortcut(0xff0d);
      button0->label(gettext("OK"));
      button0->callback( (Fl_Callback *) cb_Close);
      button1 = new Fl_Button(dx/3+2,2,dx/3-4,20);
      button1->shortcut(0xff1b);
      button1->label(gettext("Cancel"));
      button1->callback( (Fl_Callback *) cb_Cancel);
      menu=new Fl_Menu_Bar(0,2,3*dx/4,20);
      for (int i=0;i<sizeof(icas_xcas_menu)/sizeof(Fl_Menu_Item);i++){
	if (icas_xcas_menu[i].label())
	  icas_xcas_menu[i].label(gettext(icas_xcas_menu[i].label()));
      }
      menu->menu(icas_xcas_menu);
      string doc_prefix=giac::read_env(giac::context0);
      xcas::add_user_menu(menu,"xcasmenu",doc_prefix,Menu_Insert_ItemName); 
      xcas::use_external_browser=true;
      button2 = new Fl_Button(3*dx/4+2,2,dx/8-4,20);
      button2->label(gettext("STOP"));
      button2->tooltip(gettext("Interrupt computation"));
      button2->callback( (Fl_Callback *) cb_Kill);
      browser = new Fl_Check_Button(7*dx/8+2,2,dx/8-4,20);
#ifndef EMCC2
      browser->label(gettext("Safe help"));
      browser->tooltip(gettext("Check to display help with internal browser"));
      browser->callback( (Fl_Callback *) cb_Browser);
#endif
      w->end();
      w->resizable(w);
    }
    button0->show(); button1->show(); menu->hide(); button2->hide(); browser->hide();
    // xcas::initialize_function=load_autorecover_data;
    if (file_type==-1){
      quit_idle_function(0);
      Fl::add_idle(quit_idle_function,w);
    }
    else
      Fl::add_idle(xcas::Xcas_idle_function,0);
    // xcas::idle_function=Xcas_update_mode;
    Fl_Group::current(w); xcas::Xcas_input_focus=w;
    int dx=w->w(),dy=w->h();
    Fl_Tile * this_graph_tile=new Fl_Tile(0,25,dx,dy-25);
    Fl_Widget * wid =0,*print_wid=0;
    if (geometry){
      if (file_type==4 || (giac::ckmatrix(ge,true) && ge.subtype==giac::_SPREAD__VECT)){
	xcas::Tableur_Group * t=new xcas::Tableur_Group(0,25,dx,dy-25,20,2);
	wid=t;
	giac::gen tmp=giac::gen(giac::remove_path(giac::remove_extension(figure_filename)),contextptr);
	if (tmp.type==giac::_IDNT)
	  t->table->name=tmp;
	t->table->contextptr=(giac::context *) contextptr;
	if (giac::ckmatrix(ge,true) && ge.subtype==giac::_SPREAD__VECT){
	  t->table->set_matrix(*ge._VECTptr,true,false); // don't reeval!
	  // t->table->paste(*ge._VECTptr,false); // don't reeval!
	}
	if (t->table->filename)
	  delete t->table->filename;
	if ( (t->table->filename = new std::string(figure_filename)) )
	  t->fname->label(t->table->filename->c_str());
	t->table->update_status();
      }
      else {
	wid=new xcas::Figure(0,25,dx,dy-25,20,(is3d(ge)||file_type==3));
	if (wid)
	  print_wid=((xcas::Figure *)wid)->geo;
      }
    }
    else if (file_type==5){
      button0->hide(); button1->hide(); menu->show(); button2->show(); browser->show();
      xcas::History_Fold * w =new xcas::History_Fold(0,25,dx,dy-25,-1);
      w->label("");
      w->labelfont(FL_HELVETICA);
      w->pack->labelfont(FL_HELVETICA);
      w->end();
      w->pack->contextptr = (giac::context *) contextptr;
      w->pack->labelsize(18);
      w->pack->eval=xcas::Xcas_eval;
      w->pack->_insert=xcas::Xcas_pack_insert;
      w->pack->_select=xcas::Xcas_pack_select;  
      Fl::remove_idle(xcas::Xcas_idle_function,w);
      w->pack->insert_before(-1,true,0);
      Fl::add_idle(xcas::Xcas_idle_function,0);
      w->pack->add_entry(w->pack->children());
      w->pack->clear_modified();
      w->pack->focus(w->pack->children()-1,true);
      print_wid=wid=w;
    }
    else {
      int t=graph_output_type(ge);
      if (t==4 || file_type==4){
	xcas::Turtle * tu=new xcas::Turtle(0,25,dx,dy-25);
	tu->turtleptr=&turtle_stack(contextptr);
	print_wid=wid=tu;
      }
      else {
	if (t==3||file_type==3){
#if 1 // def HAVE_LIBFLTK_GL
	  print_wid=wid=new xcas::Graph3d(0,25,dx,dy-25,"",0);
#endif
	}
	else {
	  if (t==2 || file_type==2)
	    print_wid=wid=new xcas::Graph2d(0,25,dx,dy-25);
	  else {
	    print_wid=wid=new Fl_Output(0,25,dx,dy-25);
	    ((Fl_Output *) (wid))->value("No suitable widget");
	  }
	}
      }
    }
    if (xcas::Graph2d3d * this_graph = dynamic_cast<xcas::Graph2d3d *>(wid)){
      this_graph->add(ge);
      this_graph->autoscale();
      this_graph->update_infos(ge,contextptr);
    }
    if (xcas::Figure * fig=dynamic_cast<xcas::Figure *>(wid)){
      fig->rename(figure_filename.c_str());
      fig->geo->hp->remove_entry(0);
      if (!is_undef(ge))
	fig->geo->add(ge);
      // put g in figure history_pack
      if (g.type!=giac::_VECT){
	if (!is_undef(ge)){
	  xcas::Multiline_Input_tab * mi = dynamic_cast<xcas::Multiline_Input_tab * >(xcas::new_question_multiline_input(100,20));
	  mi->set_g(g);
	  fig->geo->hp->add_entry(-1,mi);
	}
      }
      else {
	giac::const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
	for (;it!=itend;++it){
	  xcas::Multiline_Input_tab * mi = dynamic_cast<xcas::Multiline_Input_tab * >(xcas::new_question_multiline_input(100,20));
	  mi->set_g(*it);
	  fig->geo->hp->add_entry(-1,mi);
	}
      }
      if (fig->geo->hp->children()==0){
	fig->geo->hp->add_entry(-1);
	fig->geo->set_mode(0,0,1);
	fig->geo->approx=true;
	fig->mode->value("point");
      }
      fig->geo->hp->update();
    }
    this_graph_tile->end();
    w->show();
    w->hotspot(w);
    fltk_return_value=-1;
    Fl::run();
    if (file_type==-1)
      Fl::remove_idle(quit_idle_function,w);
    else
      Fl::remove_idle(xcas::Xcas_idle_function,0);
    w->show();
    if (!fltk_return_value || file_type==5){
      if (xcas::History_Fold * hf=dynamic_cast<xcas::History_Fold *>(wid)){
	if (hf->pack->_modified){
	  int i=fl_ask(gettext("History modified. Save?"));
	  if (i)
	    hf->pack->save(0);
	}
      }
      if (xcas::Figure * fig=dynamic_cast<xcas::Figure *>(wid)){
	if (fig->geo->hp->_modified && !figure_filename.empty()){
	  int i=fl_ask("Figure modified. Save?");
	  if (i)
	    fig->save_figure_as(figure_filename);
	}
      }
      if (xcas::Tableur_Group * t=dynamic_cast<xcas::Tableur_Group * >(wid)){
	if (t->table->changed_ && !t->table->filename->empty()){
	  int i=fl_ask("Sheet modified. Save?");
	  if (i){
	    ofstream of(t->table->filename->c_str());
	    if (!of)
	      fl_message("%s","Write error");
	    else
	      of << giac::gen(t->table->m,giac::_SPREAD__VECT) << '\n';
	    of.close();
	  }
	}
	else {
	  ge=t->table->m;
	  ge.subtype=giac::_SPREAD__VECT;
	}
      }
      // now try to print the widget
      if (!print_wid)
	print_wid=wid;
      print_wid->resize(0,0,dx,dy);
      if (xcas::Figure * fig=dynamic_cast<xcas::Figure *>(wid)){
	fig->geo->orthonormalize();
      }
      if (!filename.empty() 
	  // && !figure_filename.empty()
	  )
	xcas::widget_ps_print(print_wid,filename,true,0,false,true,false); // don't ask user for pixel size
    }
    w->hide();
    Fl::wait(0.001);
    w->remove(this_graph_tile);
    delete wid;
    delete this_graph_tile;
    delete button0;
    delete button1;
    delete button2;
    delete browser;
    delete w;
    return !fltk_return_value;
#else // HAVE_LIBFLTK
    return false;
#endif
  }

#ifdef HAVE_LIBFLTK
  // Font selection, borrowed from FLTK tests font.cxx
  Fl_Window *form=0;

  class FontDisplay : public Fl_Widget {
    void draw();
  public:
    int font, size;
    FontDisplay(Fl_Boxtype B, int X, int Y, int W, int H, const char* L = 0) :
      Fl_Widget(X,Y,W,H,L) {box(B); font = 0; size = 14;}
  };
  void FontDisplay::draw() {
    draw_box();
    fl_font((Fl_Font)font, size);
    fl_color(FL_BLACK);
    fl_draw(label(), x()+3, y()+3, w()-6, h()-6, align());
  }
  
  FontDisplay *textobj=0;
  
  Fl_Hold_Browser *fontobj=0, *sizeobj=0;
  
#define FLTK_FONT_MAX 256
  int *sizes[FLTK_FONT_MAX];
  int numsizes[FLTK_FONT_MAX];
  int pickedsize = 14;
  
  void font_cb(Fl_Widget *, long) {
    int fn = fontobj->value();
    if (!fn) return;
    fn--;
    textobj->font = fn;
    sizeobj->clear();
    int n = numsizes[fn];
    int *s = sizes[fn];
    if (!n) {
      // no sizes
    } else if (s[0] == 0) {
      // many sizes;
      int j = 1;
      for (int i = 1; i<64 || i<s[n-1]; i++) {
	char buf[20];
	if (j < n && i==s[j]) {sprintf(buf,"@b%d",i); j++;}
	else sprintf(buf,"%d",i);
	sizeobj->add(buf);
      }
      sizeobj->value(pickedsize);
    } else {
      // some sizes
      int w = 0;
      for (int i = 0; i < n; i++) {
	if (s[i]<=pickedsize) w = i;
	char buf[20];
	sprintf(buf,"@b%d",s[i]);
	sizeobj->add(buf);
      }
      sizeobj->value(w+1);
    }
    textobj->redraw();
  }
  
  void size_cb(Fl_Widget *, long) {
    int i = sizeobj->value();
    if (!i) return;
    const char *c = sizeobj->text(i);
    while (*c < '0' || *c > '9') c++;
    pickedsize = atoi(c);
    textobj->size = pickedsize;
    textobj->redraw();
  }

  bool get_font(Fl_Font & police,int & taille){
#ifdef EMCC2
    taille=EM_ASM_INT({
        let f=Number(window.prompt('New fontsize?',$0));
        return f;
      },taille);
    return taille>=12 && taille<=48;
#else
    static Fl_Return_Button * button0 = 0 ;
    static Fl_Button * button1 =0;
    static char label[400];
    Fl::scheme(NULL);
    if (!form){
      Fl_Group::current(0);
      form = new Fl_Window(550,370);
      strcpy(label, gettext("What a nice font"));
      int i = strlen(label);
      uchar c;
      label[i++] = char(10);
      for (c = ' '+1; c < 127; c++,i++) {if (!(c&0x1f)) label[i]=' '; else label[i]=c;}
      label[i++] = char(10);
      for (c = 0xA1; c; c++,i++) {if (!(c&0x1f)) label[i]=' '; else label[i]=c;}
      label[i] = 0;
      button0 = new Fl_Return_Button(100,10,100,20);
      button0->shortcut(0xff0d);
      button0->label(gettext("OK"));
      button1 = new Fl_Button(300,10,100,20);
      button1->shortcut(0xff1b);
      button1->label(gettext("Cancel"));
      
      textobj = new FontDisplay(FL_FRAME_BOX,10,40,530,140,label);
      textobj->align(FL_ALIGN_TOP|FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_CLIP);
      textobj->color(9,47);
      fontobj = new Fl_Hold_Browser(10, 190, 390, 170);
      fontobj->box(FL_FRAME_BOX);
      fontobj->color(53,3);
      fontobj->callback(font_cb);
      form->resizable(fontobj);
      sizeobj = new Fl_Hold_Browser(410, 190, 130, 170);
      sizeobj->box(FL_FRAME_BOX);
      sizeobj->color(53,3);
      sizeobj->callback(size_cb);
      form->end();
      int k = min(fonts_available,FLTK_FONT_MAX);
      for (i = 0; i < k; i++) {
	int t; const char *name = Fl::get_font_name((Fl_Font)i,&t);
	char buffer[128];
	if (t) {
	  char *p = buffer;
	  if (t & FL_BOLD) {*p++ = '@'; *p++ = 'b';}
	  if (t & FL_ITALIC) {*p++ = '@'; *p++ = 'i';}
	  strcpy(p,name);
	  name = buffer;
	}
	fontobj->add(name);
	int *s; int n = Fl::get_font_sizes((Fl_Font)i, s);
	numsizes[i] = n;
	if (n) {
	  sizes[i] = new int[n];
	  for (int j=0; j<n; j++) sizes[i][j] = s[j];
	}
      }
    }
    pickedsize = taille;
    textobj->size = pickedsize;
    fontobj->value(police+1);
    font_cb(fontobj,0);
    form->set_modal();
    form->show();
    form->hotspot(form);
    Fl::focus(form);
    int r=-1;
    for (;;){
      Fl_Widget *o = Fl::readqueue();
      if (!o) Fl::wait();
      else {
	if (o==form){ r=1; break; }
	if (o == button0) {r = 0; break;}
	if (o == button1) {r = 1; break;}
      }
    }
    form->hide();
    if (r)
      return false;
    police=Fl_Font(textobj->font);
    taille=textobj->size;
    return true;
#endif
  }

  /*
  void test_alert(){
    string s;
    for (int i=0;i<256;i++)
      s+=char(i);
    fl_alert("%s",s.c_str());
  }
  */

#endif // HAVE_LIBFLTK

#ifndef NO_NAMESPACE_XCAS
} // namespace xcas
#endif // ndef NO_NAMESPACE_XCAS


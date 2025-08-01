// generated by Fast Light User Interface Designer (fluid) version 1.0300

#ifndef hist_h
#define hist_h
#include <FL/Fl.H>
#include <algorithm>
#include "qrcodegen.h"
#include "Xcas1.h"
#include "Help1.h"
#include <FL/fl_ask.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Bitmap.H>
#include <FL/fl_show_colormap.H>
#include "Input.h"
#include "Editeur.h"
#include "Equation.h"
#include "History.h"
#include "Tableur.h"
#include "Cfg.h"
#include "Python.h"
#include <giac/giac.h>
#include <giac/misc.h>
void Xcas_qrresize();
void Xcas_alt_ctrl_cb(int i);
Fl_Widget * Xcas_current_session();
std::string Xcas_current_session_name();
const giac::context * Xcas_get_context();
void Xcas_Keyboard_Switch(unsigned u);
int hist_max(int i,int j);
int hist_min(int i,int j);
bool Xcas_save_all(Fl_Group * wid);
void latex_save_DispG(const char * filename);
void a_propos();
void rpn_button(xcas::No_Focus_Button * wid,int i);
void show_rpn_menu(unsigned i);
void Xcas_resize_mainwindow();
std::string Xcas_fltkstring();
void Xcas_qrupdate(int i);
void Xcas_change_labelsize(int i);
giac::gen Xcas_widget_size(const giac::gen & g,const giac::context * cptr);
void make_history();
void load_history(int mws);
void load_filename(const char * filename,bool modified);
void cb_Insert_Example(Fl_Widget * w , void*);
void cb_Insert_ItemName(Fl_Widget * w , void*);
void cb_Assistant_ItemName(Fl_Widget * wid , void* ptr);
void load_autorecover_data();
void gnuplot_setview();
std::string Xcas_browser_name();
void Xcas_load_general_setup();
void Xcas_update_mode();
void Xcas_update_mode_level(int mode);
void Xcas_save_config(const giac::context * contextptr);
void Xcas_emscripten_main_loop();
#include <FL/Fl_Window.H>
extern Fl_Window *Xcas_Main_Window_;
#include <FL/Fl_Menu_Bar.H>
extern Fl_Menu_Bar *Xcas_main_menu;
extern xcas::Xcas_Tabs *Xcas_Main_Tab;
#include <FL/Fl_Group.H>
extern Fl_Group *Xcas_Keyboard_Group;
extern Fl_Group *Xcas_Alpha_Keyboard;
extern xcas::No_Focus_Button *Xcas_a_key;
extern xcas::No_Focus_Button *Xcas_b_key;
extern xcas::No_Focus_Button *Xcas_c_key;
extern xcas::No_Focus_Button *Xcas_d_key;
extern xcas::No_Focus_Button *Xcas_e_key;
extern xcas::No_Focus_Button *Xcas_f_key;
extern xcas::No_Focus_Button *Xcas_g_key;
extern xcas::No_Focus_Button *Xcas_h_key;
extern xcas::No_Focus_Button *Xcas_i_key;
extern xcas::No_Focus_Button *Xcas_j_key;
extern xcas::No_Focus_Button *Xcas_k_key;
extern xcas::No_Focus_Button *Xcas_l_key;
extern xcas::No_Focus_Button *Xcas_m_key;
extern xcas::No_Focus_Button *Xcas_n_key;
extern xcas::No_Focus_Button *Xcas_o_key;
extern xcas::No_Focus_Button *Xcas_p_key;
extern xcas::No_Focus_Button *Xcas_q_key;
extern xcas::No_Focus_Button *Xcas_r_key;
extern xcas::No_Focus_Button *Xcas_s_key;
extern xcas::No_Focus_Button *Xcas_t_key;
extern xcas::No_Focus_Button *Xcas_u_key;
extern xcas::No_Focus_Button *Xcas_v_key;
extern xcas::No_Focus_Button *Xcas_w_key;
extern xcas::No_Focus_Button *Xcas_x_key;
extern xcas::No_Focus_Button *Xcas_y_key;
extern xcas::No_Focus_Button *Xcas_z_key;
extern xcas::No_Focus_Button *Xcas_Majuscule_key;
extern xcas::No_Focus_Button *Xcas_Greek_key;
extern xcas::No_Focus_Button *Xcas_Double_quote;
extern xcas::No_Focus_Button *Xcas_espace_key;
extern Fl_Group *Xcas_Scientific_Keyboard;
extern Fl_Group *Xcas_Delim_keyboard;
extern Fl_Menu_Bar *Xcas_calculus_group;
extern xcas::No_Focus_Button *Xcas_Keyboard_shift;
extern xcas::No_Focus_Button *Xcas_Equal_button;
extern xcas::No_Focus_Button *Xcas_Sto;
extern xcas::No_Focus_Button *Xcas_Superieur;
extern xcas::No_Focus_Button *Xcas_Parentheses;
extern xcas::No_Focus_Button *Xcas_Virgule;
extern xcas::No_Focus_Button *Xcas_Keyboard_rp;
extern Fl_Group *Trig_keyboard;
extern xcas::No_Focus_Button *Xcas_Sinus_button;
extern xcas::No_Focus_Button *Xcas_Cosinus_button;
extern xcas::No_Focus_Button *Xcas_Tangeant_button;
extern xcas::No_Focus_Button *Xcas_Racine_carree;
extern xcas::No_Focus_Button *Xcas_Variable_x;
extern xcas::No_Focus_Button *Xcas_Variable_y;
extern xcas::No_Focus_Button *Xcas_Variable_z;
extern xcas::No_Focus_Button *Xcas_Exp_button;
extern xcas::No_Focus_Button *Xcas_Cst_i;
extern xcas::No_Focus_Button *Xcas_Cst_pi;
extern xcas::No_Focus_Button *Xcas_Puissance;
extern xcas::No_Focus_Button *Xcas_Diff;
extern xcas::No_Focus_Button *Xcas_Simplify;
extern xcas::No_Focus_Button *Xcas_Approx;
extern xcas::No_Focus_Button *Xcas_Ln10_button;
extern Fl_Group *Numeric_numbers;
extern xcas::No_Focus_Button *Xcas_Un;
extern xcas::No_Focus_Button *Xcas_Deux;
extern xcas::No_Focus_Button *Xcas_Trois;
extern xcas::No_Focus_Button *Xcas_Quatre;
extern xcas::No_Focus_Button *Xcas_Cinq;
extern xcas::No_Focus_Button *Xcas_Six;
extern xcas::No_Focus_Button *Xcas_Sept;
extern xcas::No_Focus_Button *Xcas_Huit;
extern xcas::No_Focus_Button *Xcas_Neuf;
extern xcas::No_Focus_Button *Xcas_Zero;
extern xcas::No_Focus_Button *Xcas_Point;
extern xcas::No_Focus_Button *Xcas_EEX;
extern xcas::No_Focus_Button *Xcas_Plus;
extern xcas::No_Focus_Button *Xcas_Moins;
extern xcas::No_Focus_Button *Xcas_Fois;
extern xcas::No_Focus_Button *Xcas_Divise;
extern Fl_Group *Kbd_control;
extern xcas::No_Focus_Button *Xcas_Undo_Key;
extern xcas::No_Focus_Button *Xcas_Echap;
extern xcas::No_Focus_Button *Xcas_Alpha;
extern xcas::No_Focus_Button *Xcas_Cmds;
extern xcas::No_Focus_Button *Xcas_Msg;
extern xcas::No_Focus_Button *Xcas_b7;
extern xcas::No_Focus_Button *Xcas_Ctrl;
extern xcas::No_Focus_Button *Xcas_close_keyboard;
extern xcas::No_Focus_Button *Xcas_main_del_button;
extern xcas::No_Focus_Button *Xcas_copy_paste_button;
extern xcas::No_Focus_Button *Xcas_main_enter_button;
extern xcas::No_Focus_Button *Xcas_main_left_button;
extern xcas::No_Focus_Button *Xcas_main_down_button;
extern xcas::No_Focus_Button *Xcas_main_up_button;
extern xcas::No_Focus_Button *Xcas_main_right_button;
extern xcas::No_Focus_Button *Xcas_main_tab_button;
extern Fl_Group *Xcas_Bandeau_Keys;
extern Fl_Group *Xcas_Bandeau_Keys_Group;
extern xcas::No_Focus_Button *Xcas_PREV_Key;
extern xcas::No_Focus_Button *Xcas_F1_Key;
extern xcas::No_Focus_Button *Xcas_F2_Key;
extern xcas::No_Focus_Button *Xcas_F3_Key;
extern xcas::No_Focus_Button *Xcas_F4_Key;
extern xcas::No_Focus_Button *Xcas_F5_Key;
extern xcas::No_Focus_Button *Xcas_F6_Key;
extern xcas::No_Focus_Button *Xcas_NXT_Key;
extern xcas::No_Focus_Button *Xcas_CST_Key;
extern xcas::No_Focus_Button *Xcas_VAR_Key;
extern xcas::No_Focus_Button *Xcas_Home_button;
extern xcas::No_Focus_Button *Xcas_close_bandeau;
extern Fl_Group *Xcas_Messages;
#include <FL/Fl_Scroll.H>
extern Fl_Scroll *Xcas_Messages_Scroll;
extern xcas::Enlargable_Multiline_Output *Xcas_parse_error_output;
extern xcas::No_Focus_Button *Xcas_close_messages;
#include <FL/Fl_Double_Window.H>
extern Fl_Double_Window *Xcas_General_Setup;
#include <FL/Fl_Menu_Button.H>
extern Fl_Menu_Button *Xcas_Level;
#include <FL/Fl_Output.H>
extern Fl_Output *Xcas_level_output;
#include <FL/Fl_Input.H>
extern Fl_Input *Xcas_html_browser;
#include <FL/Fl_Check_Button.H>
extern Fl_Check_Button *Xcas_automatic_help_browser;
extern Fl_Check_Button *Xcas_automatic_completion_browser;
#include <FL/Fl_Value_Input.H>
extern Fl_Value_Input *Xcas_autosave_time;
extern Fl_Value_Input *Xcas_default_rows;
extern Fl_Value_Input *Xcas_default_cols;
#include <FL/Fl_Return_Button.H>
extern Fl_Return_Button *Xcas_general_setup_save;
#include <FL/Fl_Button.H>
extern Fl_Button *Xcas_general_setup_close;
extern Fl_Group *Print_config;
extern Fl_Menu_Button *Xcas_Page_format;
extern Fl_Output *Xcas_Page_Format_Output;
extern Fl_Check_Button *Xcas_Printer_landscape;
extern Fl_Input *Xcas_ps_preview;
extern Fl_Input *Xcas_proxy;
extern Fl_Button *Xcas_All_Fonts;
extern Fl_Check_Button *Xcas_stepbystep;
extern Fl_Check_Button *Xcas_tooltip_disabled;
extern Fl_Check_Button *Xcas_disable_try_parse_test_i;
extern Fl_Double_Window *Xcas_Script_Window;
extern xcas::DispG_Window *Xcas_DispG_Window_;
extern Fl_Menu_Bar *Xcas_DispG_Menu;
extern Fl_Button *Xcas_DispG_Cancel_;
#include <FL/Fl_Tile.H>
extern Fl_Tile *Xcas_DispG_Tile;
extern xcas::Graph2d *Xcas_DispG_;
extern xcas::Equation *Xcas_PrintG_;
extern Fl_Button *Xcas_DispG_ClrGraph;
Fl_Window* Xcas_run(int argc,char ** argv);
extern unsigned char menu_Xcas_main_menu_i18n_done;
extern Fl_Menu_Item menu_Xcas_main_menu[];
#define Xcas_file_menu (menu_Xcas_main_menu+0)
#define Xcas_new_session (menu_Xcas_main_menu+1)
#define Xcas_open (menu_Xcas_main_menu+2)
#define Xcas_open_session (menu_Xcas_main_menu+3)
#define Xcas_open_recovery (menu_Xcas_main_menu+5)
#define Xcas_Import (menu_Xcas_main_menu+6)
#define Xcas_open_ti83 (menu_Xcas_main_menu+7)
#define Xcas_open_casio (menu_Xcas_main_menu+8)
#define Xcas_open_nws (menu_Xcas_main_menu+9)
#define Xcas_open_numworks (menu_Xcas_main_menu+10)
#define Xcas_open_nspire (menu_Xcas_main_menu+11)
#define Xcas_open_maple (menu_Xcas_main_menu+12)
#define Xcas_open_ti89 (menu_Xcas_main_menu+13)
#define Xcas_open_v200 (menu_Xcas_main_menu+14)
#define Xcas_Clone (menu_Xcas_main_menu+16)
#define Xcas_CloneOnline1 (menu_Xcas_main_menu+17)
#define Xcas_CloneOffline1 (menu_Xcas_main_menu+18)
#define Xcas_CloneOnline3 (menu_Xcas_main_menu+19)
#define Xcas_CloneOnline2 (menu_Xcas_main_menu+20)
#define Xcas_CloneOffline2 (menu_Xcas_main_menu+21)
#define Xcas_QRcode3 (menu_Xcas_main_menu+22)
#define Xcas_QRcode0 (menu_Xcas_main_menu+23)
#define Xcas_QRcode1 (menu_Xcas_main_menu+24)
#define Xcas_QRcode2 (menu_Xcas_main_menu+25)
#define Xcas_Insert (menu_Xcas_main_menu+27)
#define Xcas_Insert_Session (menu_Xcas_main_menu+28)
#define Xcas_Firefox_Insert_Session (menu_Xcas_main_menu+29)
#define Xcas_Insert_Figure (menu_Xcas_main_menu+30)
#define Xcas_Insert_Tableur (menu_Xcas_main_menu+31)
#define Xcas_Insert_Program (menu_Xcas_main_menu+32)
#define Xcas_save_current_session (menu_Xcas_main_menu+34)
#define Xcas_save_current_session_as (menu_Xcas_main_menu+35)
#define Xcas_save_all_sessions (menu_Xcas_main_menu+36)
#define Xcas_Numworks_menu (menu_Xcas_main_menu+37)
#define Xcas_NNumworks_menu (menu_Xcas_main_menu+38)
#define Xcas_nnumworks_doc (menu_Xcas_main_menu+39)
#define Xcas_open_nnumworks_xws (menu_Xcas_main_menu+40)
#define Xcas_send_nnumworks_xws (menu_Xcas_main_menu+41)
#define Xcas_send_nnumworks_prog (menu_Xcas_main_menu+42)
#define Xcas_open_nnumworks_calc (menu_Xcas_main_menu+43)
#define Xcas_Export_nnumworks_calc (menu_Xcas_main_menu+44)
#define Xcas_nnumworks_backup (menu_Xcas_main_menu+45)
#define Xcas_nnumworks_restore (menu_Xcas_main_menu+46)
#define Xcas_Handle_nnumworks_flash (menu_Xcas_main_menu+47)
#define Xcas_nnumworks_installb (menu_Xcas_main_menu+48)
#define Xcas_nnumworks_install (menu_Xcas_main_menu+49)
#define Xcas_Unlocked_Numworks_menu (menu_Xcas_main_menu+51)
#define Xcas_nws_doc (menu_Xcas_main_menu+52)
#define Xcas_open_numworks_xws (menu_Xcas_main_menu+53)
#define Xcas_send_numworks_xws (menu_Xcas_main_menu+54)
#define Xcas_send_numworks_prog (menu_Xcas_main_menu+55)
#define Xcas_open_nws_calc (menu_Xcas_main_menu+56)
#define Xcas_Export_nws_calc (menu_Xcas_main_menu+57)
#define Xcas_nw_backup (menu_Xcas_main_menu+58)
#define Xcas_nw_restore (menu_Xcas_main_menu+59)
#define Xcas_Handle_nws_flash (menu_Xcas_main_menu+60)
#define Xcas_nw_install (menu_Xcas_main_menu+61)
#define Xcas_nw_update (menu_Xcas_main_menu+62)
#define Xcas_nw_alpha (menu_Xcas_main_menu+63)
#define Xcas_nw_rescue (menu_Xcas_main_menu+64)
#define Xcas_nw_certify (menu_Xcas_main_menu+65)
#define Xcas_nw_certify_overwrite (menu_Xcas_main_menu+66)
#define Xcas_Export (menu_Xcas_main_menu+69)
#define Xcas_Export_Khicas_TI83 (menu_Xcas_main_menu+70)
#define Xcas_Export_Khicas_Casio (menu_Xcas_main_menu+71)
#define Xcas_Export_Khicas_Numworks (menu_Xcas_main_menu+72)
#define Xcas_Export_Khicas_Nspire (menu_Xcas_main_menu+73)
#define Xcas_Export_nws (menu_Xcas_main_menu+74)
#define Xcas_Export_Xcas (menu_Xcas_main_menu+75)
#define Xcas_Export_XcasPy (menu_Xcas_main_menu+76)
#define Xcas_Export_Maple (menu_Xcas_main_menu+77)
#define Xcas_Export_Mupad (menu_Xcas_main_menu+78)
#define Xcas_Export_TI (menu_Xcas_main_menu+79)
#define Xcas_Close (menu_Xcas_main_menu+81)
#define Xcas_Print (menu_Xcas_main_menu+82)
#define Xcas_Print_Preview (menu_Xcas_main_menu+83)
#define Xcas_Print_Printer (menu_Xcas_main_menu+84)
#define Xcas_Print_Preview_Selected (menu_Xcas_main_menu+85)
#define Xcas_Print_Latex (menu_Xcas_main_menu+87)
#define Xcas_LaTeX_Print_Preview (menu_Xcas_main_menu+88)
#define Xcas_LaTeX_Print_Printer (menu_Xcas_main_menu+89)
#define Xcas_LaTeX_Print_Selection (menu_Xcas_main_menu+90)
#define Xcas_screen_capture (menu_Xcas_main_menu+92)
#define Xcas_do_nothing1 (menu_Xcas_main_menu+93)
#define Xcas_create_links (menu_Xcas_main_menu+94)
#define Xcas_quit_update (menu_Xcas_main_menu+95)
#define Xcas_do_nothing2 (menu_Xcas_main_menu+96)
#define Xcas_quit (menu_Xcas_main_menu+97)
#define Xcas_Edit (menu_Xcas_main_menu+99)
#define Xcas_Execute_Worksheet (menu_Xcas_main_menu+100)
#define Xcas_Execute_Worksheet_Delay (menu_Xcas_main_menu+101)
#define Xcas_Execute_Below (menu_Xcas_main_menu+102)
#define Xcas_Remove_Answers (menu_Xcas_main_menu+103)
#define Xcas_Undo (menu_Xcas_main_menu+104)
#define Xcas_Redo (menu_Xcas_main_menu+105)
#define Xcas_Copy (menu_Xcas_main_menu+106)
#define Xcas_Paste (menu_Xcas_main_menu+107)
#define Xcas_Delete (menu_Xcas_main_menu+108)
#define Xcas_Tex_Selection (menu_Xcas_main_menu+109)
#define Xcas_Add_Entry1 (menu_Xcas_main_menu+110)
#define Xcas_Add_Parameter (menu_Xcas_main_menu+111)
#define Xcas_Insert_Newline (menu_Xcas_main_menu+112)
#define Xcas_Merge (menu_Xcas_main_menu+113)
#define Xcas_Add_HF (menu_Xcas_main_menu+114)
#define Xcas_Fold (menu_Xcas_main_menu+115)
#define Xcas_Flatten (menu_Xcas_main_menu+116)
#define Xcas_main_configuration (menu_Xcas_main_menu+118)
#define Xcas_cas_config_ (menu_Xcas_main_menu+119)
#define Xcas_graph_config_ (menu_Xcas_main_menu+120)
#define Xcas_general_config_ (menu_Xcas_main_menu+121)
#define Xcas_config_program (menu_Xcas_main_menu+122)
#define Xcas_Set_Xcas0 (menu_Xcas_main_menu+123)
#define Xcas_Set_Python0 (menu_Xcas_main_menu+124)
#define Xcas_Set_Python0xor (menu_Xcas_main_menu+125)
#define Xcas_Set_Maple0 (menu_Xcas_main_menu+126)
#define Xcas_Set_Mupad0 (menu_Xcas_main_menu+127)
#define Xcas_Set_TI0 (menu_Xcas_main_menu+128)
#define Xcas_show_something (menu_Xcas_main_menu+130)
#define Xcas_show_DispG (menu_Xcas_main_menu+131)
#define Xcas_show_keyboard (menu_Xcas_main_menu+132)
#define Xcas_show_bandeau (menu_Xcas_main_menu+133)
#define Xcas_show_msg (menu_Xcas_main_menu+134)
#define Xcas_show_script_window (menu_Xcas_main_menu+135)
#define Xcas_Hide_Something (menu_Xcas_main_menu+137)
#define Xcas_hide_DispG (menu_Xcas_main_menu+138)
#define Xcas_hide_keyboard (menu_Xcas_main_menu+139)
#define Xcas_hide_bandeau (menu_Xcas_main_menu+140)
#define Xcas_hide_msg (menu_Xcas_main_menu+141)
#define Xcas_index_language (menu_Xcas_main_menu+143)
#define Xcas_index_Francais (menu_Xcas_main_menu+144)
#define Xcas_index_English (menu_Xcas_main_menu+145)
#define Xcas_index_Espanol (menu_Xcas_main_menu+146)
#define Xcas_index_Greek (menu_Xcas_main_menu+147)
#define Xcas_index_Chinese (menu_Xcas_main_menu+148)
#define Xcas_index_German (menu_Xcas_main_menu+149)
#define Xcas_Colors (menu_Xcas_main_menu+151)
#define Xcas_background_color_item (menu_Xcas_main_menu+152)
#define Xcas_editor_color_item (menu_Xcas_main_menu+153)
#define Xcas_editor_background_color_item (menu_Xcas_main_menu+154)
#define Xcas_input_text_color (menu_Xcas_main_menu+155)
#define Xcas_input_text_background_color (menu_Xcas_main_menu+156)
#define Xcas_comment_color_item (menu_Xcas_main_menu+157)
#define Xcas_comment_background_color_item (menu_Xcas_main_menu+158)
#define Xcas_log_color_item (menu_Xcas_main_menu+159)
#define Xcas_log_background_color_item (menu_Xcas_main_menu+160)
#define Xcas_equation_color_item (menu_Xcas_main_menu+161)
#define Xcas_equation_background_color_item (menu_Xcas_main_menu+162)
#define Xcas_change_current_fontsize (menu_Xcas_main_menu+164)
#define Xcas_change_fontsize (menu_Xcas_main_menu+165)
#define Xcas_browser (menu_Xcas_main_menu+166)
#define Save_config (menu_Xcas_main_menu+167)
#define Xcas_help_menu (menu_Xcas_main_menu+169)
#define Xcas_help_index (menu_Xcas_main_menu+170)
#define Xcas_help_find (menu_Xcas_main_menu+171)
#define Xcas_help_casinter (menu_Xcas_main_menu+172)
#define Xcas_help_fiches (menu_Xcas_main_menu+173)
#define Xcas_help_manuals (menu_Xcas_main_menu+174)
#define Xcas_help_CAS (menu_Xcas_main_menu+175)
#define Xcas_help_graphtheory (menu_Xcas_main_menu+176)
#define Xcas_help_algo (menu_Xcas_main_menu+177)
#define Xcas_help_algo_pdf (menu_Xcas_main_menu+178)
#define Xcas_help_Geo (menu_Xcas_main_menu+179)
#define Xcas_help_Prog (menu_Xcas_main_menu+180)
#define Xcas_help_Tableur (menu_Xcas_main_menu+181)
#define Xcas_help_Tortue (menu_Xcas_main_menu+182)
#define Xcas_help_Exercices (menu_Xcas_main_menu+183)
#define Xcas_help_Amusement (menu_Xcas_main_menu+184)
#define Xcas_help_PARI (menu_Xcas_main_menu+185)
#define Xcas_help_internet (menu_Xcas_main_menu+187)
#define Xcas_help_forum (menu_Xcas_main_menu+188)
#define Xcas_help_lycee_card (menu_Xcas_main_menu+189)
#define Xcas_help_lycee (menu_Xcas_main_menu+190)
#define Xcas_help_algo_lycee (menu_Xcas_main_menu+191)
#define Xcas_help_connan (menu_Xcas_main_menu+192)
#define Xcas_help_alb (menu_Xcas_main_menu+193)
#define Xcas_help_cheval (menu_Xcas_main_menu+194)
#define Xcas_help_han (menu_Xcas_main_menu+195)
#define Xcas_help_oim (menu_Xcas_main_menu+196)
#define Xcas_help_capes (menu_Xcas_main_menu+197)
#define Xcas_help_agreg (menu_Xcas_main_menu+198)
#define Xcas_help_agregint (menu_Xcas_main_menu+199)
#define Xcas_help_pavages (menu_Xcas_main_menu+200)
#define Xcas_help_tripletpytha (menu_Xcas_main_menu+201)
#define Xcas_help_spiralepremier (menu_Xcas_main_menu+202)
#define Xcas_help_pandigit (menu_Xcas_main_menu+203)
#define Xcas_help_load (menu_Xcas_main_menu+204)
#define Xcas_help_start (menu_Xcas_main_menu+206)
#define Xcas_help_tutorial (menu_Xcas_main_menu+207)
#define Xcas_help_solution (menu_Xcas_main_menu+208)
#define Xcas_help_start_algoseconde (menu_Xcas_main_menu+210)
#define Xcas_help_make_index (menu_Xcas_main_menu+211)
#define Xcas_a_propos (menu_Xcas_main_menu+212)
#define Xcas_help_CASmenu (menu_Xcas_main_menu+214)
#define Xcas_Add_Entry (menu_Xcas_main_menu+215)
#define Xcas_Add_Comment (menu_Xcas_main_menu+216)
#define Xcas_Add_Expression (menu_Xcas_main_menu+217)
#define Xcas_help_equations (menu_Xcas_main_menu+218)
#define Xcas_help_solve (menu_Xcas_main_menu+219)
#define Xcas_help_fsolve (menu_Xcas_main_menu+220)
#define Xcas_help_proot (menu_Xcas_main_menu+221)
#define Xcas_help_linsolve (menu_Xcas_main_menu+222)
#define Xcas_help_desolve (menu_Xcas_main_menu+223)
#define Xcas_help_odesolve (menu_Xcas_main_menu+224)
#define Xcas_help_rsolve (menu_Xcas_main_menu+225)
#define Xcas_help_calculus (menu_Xcas_main_menu+227)
#define Xcas_help_int (menu_Xcas_main_menu+228)
#define Xcas_help_diff (menu_Xcas_main_menu+229)
#define Xcas_help_limit (menu_Xcas_main_menu+230)
#define Xcas_help_ptayl (menu_Xcas_main_menu+231)
#define Xcas_help_series (menu_Xcas_main_menu+232)
#define Xcas_help_sum (menu_Xcas_main_menu+233)
#define Xcas_help_laplace (menu_Xcas_main_menu+234)
#define Xcas_help_ilaplace (menu_Xcas_main_menu+235)
#define Xcas_help_Simplify (menu_Xcas_main_menu+237)
#define Xcas_help_simplify (menu_Xcas_main_menu+238)
#define Xcas_help_normal (menu_Xcas_main_menu+239)
#define Xcas_help_ratnormal (menu_Xcas_main_menu+240)
#define Xcas_help_factor (menu_Xcas_main_menu+241)
#define Xcas_help_cfactor (menu_Xcas_main_menu+242)
#define Xcas_help_partfrac (menu_Xcas_main_menu+243)
#define Xcas_help_cpartfrac (menu_Xcas_main_menu+244)
#define Xcas_help_sep1 (menu_Xcas_main_menu+245)
#define Xcas_help_subst (menu_Xcas_main_menu+246)
#define Xcas_help_reorder (menu_Xcas_main_menu+247)
#define Xcas_help_sep2 (menu_Xcas_main_menu+248)
#define Xcas_help_lin (menu_Xcas_main_menu+249)
#define Xcas_help_tlin (menu_Xcas_main_menu+250)
#define Xcas_help_texpand (menu_Xcas_main_menu+251)
#define Xcas_help_lncollect (menu_Xcas_main_menu+252)
#define Xcas_help_exp2pow (menu_Xcas_main_menu+253)
#define Xcas_help_exp2trig (menu_Xcas_main_menu+254)
#define Xcas_help_trig2exp (menu_Xcas_main_menu+255)
#define Xcas_help_arit (menu_Xcas_main_menu+257)
#define Xcas_help_isprime (menu_Xcas_main_menu+258)
#define Xcas_help_ifactor (menu_Xcas_main_menu+259)
#define Xcas_help_iquo (menu_Xcas_main_menu+260)
#define Xcas_help_irem (menu_Xcas_main_menu+261)
#define Xcas_help_iabcuv (menu_Xcas_main_menu+262)
#define Xcas_help_ichinrem (menu_Xcas_main_menu+263)
#define Xcas_help_sep3 (menu_Xcas_main_menu+264)
#define Xcas_help_gcd (menu_Xcas_main_menu+265)
#define Xcas_help_lcm (menu_Xcas_main_menu+266)
#define Xcas_help_powmod (menu_Xcas_main_menu+267)
#define Xcas_help_sep4 (menu_Xcas_main_menu+268)
#define Xcas_help_quo (menu_Xcas_main_menu+269)
#define Xcas_help_rem (menu_Xcas_main_menu+270)
#define Xcas_help_abcuv (menu_Xcas_main_menu+271)
#define Xcas_help_chinrem (menu_Xcas_main_menu+272)
#define Xcas_help_pourcent (menu_Xcas_main_menu+273)
#define Xcas_help_GF (menu_Xcas_main_menu+274)
#define Xcas_help_linal (menu_Xcas_main_menu+276)
#define Xcas_help_matrix (menu_Xcas_main_menu+277)
#define Xcas_help_tran (menu_Xcas_main_menu+278)
#define Xcas_help_ker (menu_Xcas_main_menu+279)
#define Xcas_help_image (menu_Xcas_main_menu+280)
#define Xcas_help_inverse (menu_Xcas_main_menu+281)
#define Xcas_help_det (menu_Xcas_main_menu+282)
#define Xcas_help_charpoly (menu_Xcas_main_menu+283)
#define Xcas_help_egv (menu_Xcas_main_menu+284)
#define Xcas_help_egvl (menu_Xcas_main_menu+285)
#define Xcas_help_proba (menu_Xcas_main_menu+287)
#define Xcas_help_binomial (menu_Xcas_main_menu+288)
#define Xcas_help_comb (menu_Xcas_main_menu+289)
#define Xcas_help_normald (menu_Xcas_main_menu+290)
#define Xcas_help_seq (menu_Xcas_main_menu+291)
#define Xcas_help_rand (menu_Xcas_main_menu+292)
#define Xcas_help_ranm (menu_Xcas_main_menu+293)
#define Xcas_help_randmarkov (menu_Xcas_main_menu+294)
#define Xcas_help_markov (menu_Xcas_main_menu+295)
#define Xcas_help_cdf (menu_Xcas_main_menu+296)
#define Xcas_help_icdf (menu_Xcas_main_menu+297)
#define Xcas_proba_plot (menu_Xcas_main_menu+298)
#define Xcas_help_betad (menu_Xcas_main_menu+300)
#define Xcas_help_cauchy (menu_Xcas_main_menu+301)
#define Xcas_help_chisquare (menu_Xcas_main_menu+302)
#define Xcas_help_exponential (menu_Xcas_main_menu+303)
#define Xcas_help_fisher (menu_Xcas_main_menu+304)
#define Xcas_help_gammad (menu_Xcas_main_menu+305)
#define Xcas_help_normald2 (menu_Xcas_main_menu+306)
#define Xcas_help_student (menu_Xcas_main_menu+307)
#define Xcas_help_uniform (menu_Xcas_main_menu+308)
#define Xcas_help_weibull (menu_Xcas_main_menu+309)
#define Xcas_help_binomial2 (menu_Xcas_main_menu+312)
#define Xcas_help_negbinomial (menu_Xcas_main_menu+313)
#define Xcas_help_geometric (menu_Xcas_main_menu+314)
#define Xcas_help_poisson (menu_Xcas_main_menu+315)
#define Xcas_help_chisquare_icdf (menu_Xcas_main_menu+318)
#define Xcas_help_chisquaret (menu_Xcas_main_menu+319)
#define Xcas_help_kolmogorovd (menu_Xcas_main_menu+320)
#define Xcas_help_kolmogorovt (menu_Xcas_main_menu+321)
#define Xcas_help_normald_icdf2 (menu_Xcas_main_menu+322)
#define Xcas_help_normalt (menu_Xcas_main_menu+323)
#define Xcas_help_studentd_icdf2 (menu_Xcas_main_menu+324)
#define Xcas_help_studentt (menu_Xcas_main_menu+325)
#define Xcas_help_wilcoxont (menu_Xcas_main_menu+326)
#define Xcas_help_allcmds (menu_Xcas_main_menu+330)
#define Xcas_help_constants (menu_Xcas_main_menu+331)
#define Xcas_help_i (menu_Xcas_main_menu+332)
#define Xcas_help_e (menu_Xcas_main_menu+333)
#define Xcas_help_prog (menu_Xcas_main_menu+336)
#define Xcas_Add_Program (menu_Xcas_main_menu+337)
#define Xcas_help_debug (menu_Xcas_main_menu+338)
#define Xcas_help_graph (menu_Xcas_main_menu+340)
#define Xcas_help_attributs (menu_Xcas_main_menu+341)
#define Xcas_help_curves (menu_Xcas_main_menu+342)
#define Xcas_help_plot (menu_Xcas_main_menu+343)
#define Xcas_help_plotparam2d (menu_Xcas_main_menu+344)
#define Xcas_help_plotpolar (menu_Xcas_main_menu+345)
#define Xcas_help_plotarea (menu_Xcas_main_menu+346)
#define Xcas_help_plotimplicit (menu_Xcas_main_menu+347)
#define Xcas_help_plotcontour (menu_Xcas_main_menu+348)
#define Xcas_help_plotdensity (menu_Xcas_main_menu+349)
#define Xcas_help_graph_surfaces (menu_Xcas_main_menu+351)
#define Xcas_help_plot3d (menu_Xcas_main_menu+352)
#define Xcas_help_plotparam3d (menu_Xcas_main_menu+353)
#define Xcas_help_graph_suite (menu_Xcas_main_menu+355)
#define Xcas_help_plotseq (menu_Xcas_main_menu+356)
#define Xcas_help_plotlist (menu_Xcas_main_menu+357)
#define Xcas_help_graph_ode (menu_Xcas_main_menu+359)
#define Xcas_help_plotfield (menu_Xcas_main_menu+360)
#define Xcas_help_plotode (menu_Xcas_main_menu+361)
#define Xcas_help_iplotode (menu_Xcas_main_menu+362)
#define Xcas_help_graph_stats (menu_Xcas_main_menu+364)
#define Xcas_help_camembert (menu_Xcas_main_menu+365)
#define Xcas_help_baton (menu_Xcas_main_menu+366)
#define Xcas_help_histogram (menu_Xcas_main_menu+367)
#define Xcas_help_plotstat (menu_Xcas_main_menu+368)
#define Xcas_help_plotcdf (menu_Xcas_main_menu+369)
#define Xcas_help_moustache (menu_Xcas_main_menu+370)
#define Xcas_help_scatterplot (menu_Xcas_main_menu+371)
#define Xcas_help_polygonplot (menu_Xcas_main_menu+372)
#define Xcas_help_polygonscatterplot (menu_Xcas_main_menu+373)
#define Xcas_help_linear_regression_plot (menu_Xcas_main_menu+374)
#define Xcas_help_plotproba (menu_Xcas_main_menu+375)
#define Xcas_help_geo (menu_Xcas_main_menu+378)
#define Xcas_Add_Figure (menu_Xcas_main_menu+379)
#define Xcas_Add_Figure3d (menu_Xcas_main_menu+380)
#define Xcas_help_tableur (menu_Xcas_main_menu+382)
#define Xcas_Add_Tableur (menu_Xcas_main_menu+383)
#define Xcas_help_phys (menu_Xcas_main_menu+385)
#define Xcas_help_tortue (menu_Xcas_main_menu+387)
#define Xcas_Add_Logo (menu_Xcas_main_menu+388)
#define Xcas_help_scolaire (menu_Xcas_main_menu+390)
extern unsigned char menu_Xcas_calculus_group_i18n_done;
extern Fl_Menu_Item menu_Xcas_calculus_group[];
#define Xcas_Helpon (menu_Xcas_calculus_group+0)
#define Xcas_Alg_Menubutton (menu_Xcas_calculus_group+1)
#define Xcas_Alg_simplify (menu_Xcas_calculus_group+2)
#define Xcas_Alg_factor (menu_Xcas_calculus_group+3)
#define Xcas_Alg_cfactor (menu_Xcas_calculus_group+4)
#define Xcas_Alg_approx (menu_Xcas_calculus_group+5)
#define Xcas_Alg_exact (menu_Xcas_calculus_group+6)
#define Xcas_Alg_subst (menu_Xcas_calculus_group+7)
#define Xcas_Calc_Menubutton (menu_Xcas_calculus_group+9)
#define Xcas_Calc_diff (menu_Xcas_calculus_group+10)
#define Xcas_Calc_integrate (menu_Xcas_calculus_group+11)
#define Xcas_Calc_sum (menu_Xcas_calculus_group+12)
#define Xcas_Calc_limit (menu_Xcas_calculus_group+13)
#define Xcas_Calc_series (menu_Xcas_calculus_group+14)
#define Xcas_Plot_Menubutton (menu_Xcas_calculus_group+16)
#define Xcas_Plot_plot (menu_Xcas_calculus_group+17)
#define Xcas_Plot_plotparam (menu_Xcas_calculus_group+18)
#define Xcas_Plot_plotpolar (menu_Xcas_calculus_group+19)
#define Xcas_Plot_plotseq (menu_Xcas_calculus_group+20)
#define Xcas_Plot_histogram (menu_Xcas_calculus_group+21)
#define Xcas_Plot_scatterplot (menu_Xcas_calculus_group+22)
#define Xcas_Plot_linear_regression_plot (menu_Xcas_calculus_group+23)
#define Xcas_Plot_plotimplicit (menu_Xcas_calculus_group+24)
#define Xcas_Plot_plotfield (menu_Xcas_calculus_group+25)
#define Xcas_Prg_Menubutton (menu_Xcas_calculus_group+27)
#define Xcas_Prg_si (menu_Xcas_calculus_group+28)
#define Xcas_Prg_pour (menu_Xcas_calculus_group+29)
#define Xcas_Prg_tantque (menu_Xcas_calculus_group+30)
#define Xcas_Prg_repeter (menu_Xcas_calculus_group+31)
#define Xcas_Prg_fonction (menu_Xcas_calculus_group+32)
#define Xcas_Prg_return (menu_Xcas_calculus_group+33)
extern unsigned char menu_Xcas_Level_i18n_done;
extern Fl_Menu_Item menu_Xcas_Level[];
#define Xcas_CAS_level (menu_Xcas_Level+0)
#define Xcas_Prog_level (menu_Xcas_Level+1)
#define Xcas_Tableur_level (menu_Xcas_Level+2)
#define Xcas_Geometry_level (menu_Xcas_Level+3)
#define Xcas_Tortue_level (menu_Xcas_Level+4)
extern unsigned char menu_Xcas_Page_format_i18n_done;
extern Fl_Menu_Item menu_Xcas_Page_format[];
#define Xcas_Page_A4 (menu_Xcas_Page_format+0)
#define Xcas_Page_A5 (menu_Xcas_Page_format+1)
#define Xcas_Page_A3 (menu_Xcas_Page_format+2)
#define Xcas_Page_LETTER (menu_Xcas_Page_format+3)
#define Xcas_Page_ENVELOPE (menu_Xcas_Page_format+4)
extern unsigned char menu_Xcas_DispG_Menu_i18n_done;
extern Fl_Menu_Item menu_Xcas_DispG_Menu[];
int main(int argc,char ** argv);
#endif

/*
4coder_default_bidings.cpp - Supplies the default bindings used for default 4coder behavior.
*/

// TOP

#if !defined(FCODER_DEFAULT_BINDINGS_CPP)
#define FCODER_DEFAULT_BINDINGS_CPP

#include "4coder_default_include.cpp"
#include "personal/4coder_default_api.cpp"
#include "personal/4coder_vim.cpp"
#include "personal/4coder_render.cpp"
#include "personal/4coder_terminal.cpp"

// NOTE(allen): Users can declare their own managed IDs here.

#if !defined(META_PASS)
#include "generated/managed_id_metadata.cpp"
#endif

void
custom_layer_init(Application_Links *app){
    Thread_Context *tctx = get_thread_context(app);
    
    // NOTE(allen): setup for default framework
    default_framework_init(app);
    
    // NOTE(allen): default hooks and command maps
    set_all_default_hooks(app);
    mapping_init(tctx, &framework_mapping);
    String_ID global_map_id = vars_save_string_lit("keys_global");
    String_ID file_map_id = vars_save_string_lit("keys_file");
    String_ID code_map_id = vars_save_string_lit("keys_code");
#if OS_MAC
    setup_mac_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id);
#else
    // setup_default_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id);
#endif
	setup_essential_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id);


    default_view_get_screen_rect = view_get_screen_rect;
    view_get_screen_rect = personal_view_get_screen_rect_type;

    set_custom_hook(app, HookID_RenderCaller, personal_render_caller);
    set_custom_hook(app, HookID_WholeScreenRenderCaller, personal_whole_screen_render_caller);
    vim_setup(app, global_map_id, file_map_id, code_map_id);
}

#endif //FCODER_DEFAULT_BINDINGS

// BOTTOM


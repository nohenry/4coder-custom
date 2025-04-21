#include "4coder_default_include.cpp"
#include "4coder_default_api.cpp"

function void
personal_draw_cursor(Application_Links *app, View_ID view_id, b32 is_active_view,
                                                 Buffer_ID buffer, Text_Layout_ID text_layout_id,
                                                 f32 roundness, f32 outline_thickness){
     draw_highlight_range(app, view_id, buffer, text_layout_id, roundness);
    // if (!has_highlight_range){
        i32 cursor_sub_id = default_cursor_sub_id();
        
        i64 cursor_pos = view_get_cursor_pos(app, view_id);
        // i64 mark_pos = view_get_mark_pos(app, view_id);
        if (is_active_view){
            draw_character_block(app, text_layout_id, cursor_pos, roundness,
                                 fcolor_id(defcolor_cursor, cursor_sub_id));
            paint_text_color_pos(app, text_layout_id, cursor_pos,
                                 fcolor_id(defcolor_at_cursor));
            // draw_character_wire_frame(app, text_layout_id, mark_pos,
            //                           roundness, outline_thickness,
            //                           fcolor_id(defcolor_mark));
        }
        else{
            // draw_character_wire_frame(app, text_layout_id, mark_pos,
            //                           roundness, outline_thickness,
            //                           fcolor_id(defcolor_mark));
            draw_character_wire_frame(app, text_layout_id, cursor_pos,
                                      roundness, outline_thickness,
                                      fcolor_id(defcolor_cursor, cursor_sub_id));
        }
    // }
}

function void
personal_render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id,
                      Buffer_ID buffer, Text_Layout_ID text_layout_id,
                      Rect_f32 rect){
    ProfileScope(app, "render buffer");
    
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);
    
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    
    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    u64 cursor_roundness_100 = def_get_config_u64(app, vars_save_string_lit("cursor_roundness"));
    f32 cursor_roundness = metrics.normal_advance*cursor_roundness_100*0.01f;
    f32 mark_thickness = (f32)def_get_config_u64(app, vars_save_string_lit("mark_thickness"));
    
    // NOTE(allen): Token colorizing
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    if (token_array.tokens != 0){
        draw_cpp_token_colors(app, text_layout_id, &token_array);
        
        // NOTE(allen): Scan for TODOs and NOTEs
        b32 use_comment_keyword = def_get_config_b32(vars_save_string_lit("use_comment_keyword"));
        if (use_comment_keyword){
            Comment_Highlight_Pair pairs[] = {
                {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
                {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
            };
            draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));
        }

    }
    else{
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }
    
    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Scope highlight
    b32 use_scope_highlight = def_get_config_b32(vars_save_string_lit("use_scope_highlight"));
    if (use_scope_highlight){
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
    
    b32 use_error_highlight = def_get_config_b32(vars_save_string_lit("use_error_highlight"));
    b32 use_jump_highlight = def_get_config_b32(vars_save_string_lit("use_jump_highlight"));
    if (use_error_highlight || use_jump_highlight){
        // NOTE(allen): Error highlight
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if (use_error_highlight){
            draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                                 fcolor_id(defcolor_highlight_junk));
        }
        
        // NOTE(allen): Search highlight
        if (use_jump_highlight){
            Buffer_ID jump_buffer = get_locked_jump_buffer(app);
            if (jump_buffer != compilation_buffer){
                draw_jump_highlights(app, buffer, text_layout_id, jump_buffer,
                                     fcolor_id(defcolor_highlight_white));
            }
        }
    }
    
    // NOTE(allen): Color parens
    b32 use_paren_helper = def_get_config_b32(vars_save_string_lit("use_paren_helper"));
    if (use_paren_helper){
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
    
    // NOTE(allen): Line highlight
    b32 highlight_line_at_cursor = def_get_config_b32(vars_save_string_lit("highlight_line_at_cursor"));
    if (highlight_line_at_cursor && is_active_view){
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        draw_line_highlight(app, text_layout_id, line_number, fcolor_id(defcolor_highlight_cursor_line));
    }
    
    // NOTE(allen): Whitespace highlight
    b64 show_whitespace = false;
    view_get_setting(app, view_id, ViewSetting_ShowWhitespace, &show_whitespace);
    if (show_whitespace){
        if (token_array.tokens == 0){
            draw_whitespace_highlight(app, buffer, text_layout_id, cursor_roundness);
        }
        else{
            draw_whitespace_highlight(app, text_layout_id, &token_array, cursor_roundness);
        }
    }
    
    // NOTE(allen): Cursor
    switch (fcoder_mode){
        case FCoderMode_Original:
        {
            if (global_vim_state.mode == Vim_Mode::Normal || global_vim_state.mode == Vim_Mode::Visual || !is_active_view || global_vim_state.in_command) {
                personal_draw_cursor(app, view_id, is_active_view && !global_vim_state.in_command, buffer, text_layout_id, 0, mark_thickness);
            } else {
                // draw_notepad_style_cursor_highlight(app, view_id, buffer, text_layout_id, 0);

                i32 cursor_sub_id = default_cursor_sub_id();
                draw_character_i_bar(app, text_layout_id, cursor_pos, fcolor_id(defcolor_cursor, cursor_sub_id));
            }
        }break;
        case FCoderMode_NotepadLike:
        {
            draw_notepad_style_cursor_highlight(app, view_id, buffer, text_layout_id, cursor_roundness);
        }break;
    }
    
    // NOTE(allen): Fade ranges
    paint_fade_ranges(app, text_layout_id, buffer);
    
    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);
    
    draw_set_clip(app, prev_clip);
}

function void
personal_render_command_buffer(Application_Links *app, View_ID view_id, Face_ID face_id,
                      Buffer_ID buffer, Text_Layout_ID text_layout_id,
                      Rect_f32 rect){
    ProfileScope(app, "command buffer");
    
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);
    
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    
    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    u64 cursor_roundness_100 = def_get_config_u64(app, vars_save_string_lit("cursor_roundness"));
    f32 cursor_roundness = metrics.normal_advance*cursor_roundness_100*0.01f;
    f32 mark_thickness = (f32)def_get_config_u64(app, vars_save_string_lit("mark_thickness"));

    paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    
    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Cursor
    switch (fcoder_mode){
        case FCoderMode_Original:
        {
            if (global_vim_state.mode == Vim_Mode::Normal || global_vim_state.mode == Vim_Mode::Visual) {
                personal_draw_cursor(app, view_id, is_active_view, buffer, text_layout_id, 0, mark_thickness);
            } else {
                i32 cursor_sub_id = default_cursor_sub_id();
                draw_character_i_bar(app, text_layout_id, cursor_pos, fcolor_id(defcolor_cursor, cursor_sub_id));
            }
        }break;
        case FCoderMode_NotepadLike:
        {
            draw_notepad_style_cursor_highlight(app, view_id, buffer, text_layout_id, cursor_roundness);
        }break;
    }
    
    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);
    
    draw_set_clip(app, prev_clip);
}


function void
personal_draw_file_bar(Application_Links *app, View_ID view_id, Buffer_ID buffer, Face_ID face_id, Rect_f32 bar){
    Scratch_Block scratch(app);
    
    View_ID active_view = get_active_view(app, Access_Read);
    if (view_id == active_view || global_vim_state.in_command && global_vim_state.last_view == view_id) {
        draw_rectangle_fcolor(app, bar, 0.f, fcolor_id(defcolor_bar));
    } else {
        draw_rectangle_fcolor(app, bar, 0.f, fcolor_id(defcolor_back));
    }
    
    FColor base_color = fcolor_id(defcolor_base);
    FColor pop2_color = fcolor_id(defcolor_pop2);
    
    i64 cursor_position = view_get_cursor_pos(app, view_id);
    Buffer_Cursor cursor = view_compute_cursor(app, view_id, seek_pos(cursor_position));
    // view_get_bufferbegin
    i64 size = buffer_get_size(app, buffer);
    
    Fancy_Line list_begin = {};
    Fancy_Line list_end = {};
    String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, buffer);
    push_fancy_string(scratch, &list_begin, base_color, unique_name);

    push_fancy_stringf(scratch, &list_end, base_color, "%lld,%lld", cursor.line, cursor.col);
    if (size == 0) {
        push_fancy_stringf(scratch, &list_end, base_color, "   0%%");
    } else {
        push_fancy_stringf(scratch, &list_end, base_color, " %3lld%%", cursor_position * 100LL / size);
    }

    // view_get_cursor_pos
    
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting,
                                                     Line_Ending_Kind);
    switch (*eol_setting){
        case LineEndingKind_Binary:
        {
            push_fancy_string(scratch, &list_begin, base_color, string_u8_litexpr(" bin"));
        }break;
        
        case LineEndingKind_LF:
        {
            // push_fancy_string(scratch, &list_begin, base_color, string_u8_litexpr(" lf"));
        }break;
        
        case LineEndingKind_CRLF:
        {
            push_fancy_string(scratch, &list_begin, base_color, string_u8_litexpr(" crlf"));
        }break;
    }
    
    u8 space[3];
    {
        Dirty_State dirty = buffer_get_dirty_state(app, buffer);
        String_u8 str = Su8(space, 0, 3);
        if (dirty != 0){
            string_append(&str, string_u8_litexpr(" "));
        }
        if (HasFlag(dirty, DirtyState_UnsavedChanges)){
            string_append(&str, string_u8_litexpr("*"));
        }
        if (HasFlag(dirty, DirtyState_UnloadedChanges)){
            string_append(&str, string_u8_litexpr("!"));
        }
        push_fancy_string(scratch, &list_begin, pop2_color, str.string);
    }
    
    Vec2_f32 p = bar.p0 + V2f32(2.f, 2.f);
    draw_fancy_line(app, face_id, fcolor_zero(), &list_begin, p);

    f32 end_width = get_fancy_line_width(app, face_id, &list_end);
    Vec2_f32 p_end = V2f32(bar.x1, bar.y0) + V2f32(-2.f, 2.f) - V2f32(end_width, 0.0f);
    draw_fancy_line(app, face_id, fcolor_zero(), &list_end, p_end);
}

function void
personal_draw_status_bar(Application_Links *app, Face_ID face_id, Rect_f32 bar)
{
    Scratch_Block scratch(app);
    FColor text_color = fcolor_id(defcolor_text_default);
    View_ID active_view = get_active_view(app, Access_Read);

    i64 cursor_start = global_vim_state.visual_mark;
    i64 cursor_pos = view_get_cursor_pos(app, active_view);

    Fancy_Line list_begin = {};
    Fancy_Line list_end = {};

    switch (global_vim_state.mode) {
        case Vim_Mode::Normal:
            // push_fancy_string(scratch, &list_begin, text_color, string_u8_litexpr("INSERT"));
            break;
        case Vim_Mode::Visual:
            push_fancy_string(scratch, &list_begin, text_color, string_u8_litexpr("Visual"));
            push_fancy_stringf(scratch, &list_end, text_color, " %lld ", abs(cursor_pos - cursor_start) + 1);
            break;
        case Vim_Mode::Insert:
            push_fancy_string(scratch, &list_begin, text_color, string_u8_litexpr("Insert"));
            break;
        default:
            push_fancy_string(scratch, &list_begin, text_color, string_u8_litexpr("errm"));
            break;
    }

    Vec2_f32 p = bar.p0 + V2f32(2.f, 2.f);
    draw_fancy_line(app, face_id, fcolor_zero(), &list_begin, p);

    f32 end_width = get_fancy_line_width(app, face_id, &list_end);
    Vec2_f32 p_end = V2f32(bar.x1, bar.y0) + V2f32(-2.f, 2.f) - V2f32(end_width, 0.0f);
    draw_fancy_line(app, face_id, fcolor_zero(), &list_end, p_end);
}

function void
personal_render_caller(Application_Links *app, Frame_Info frame_info, View_ID view_id){
    ProfileScope(app, "default render caller");
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);

    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);

    // i64 setting_value;
    if (view_id == global_vim_state.command_view) {
        Rect_f32 view_rect = view_get_screen_rect(app, view_id);
        FColor back = fcolor_id(defcolor_back);
        draw_rectangle_fcolor(app, view_rect, 0.f, back);

        if (global_vim_state.in_command) {
            Rect_f32 prev_clip = draw_set_clip(app, view_rect);

            auto size = draw_string(app, face_id, str8_lit(":"), view_rect.p0, fcolor_id(defcolor_text_default));
            Rect_f32_Pair type_region = rect_split_left_right(view_rect, size.x);

            Text_Layout_ID text_layout_id = text_layout_create(app, buffer, type_region.max, {0, 0});
            personal_render_command_buffer(app, view_id, face_id, buffer, text_layout_id, type_region.max);

            text_layout_free(app, text_layout_id);
            draw_set_clip(app, prev_clip);
        } else {
            personal_draw_status_bar(app, face_id, view_rect);
        }

        return;
    } else if (view_id == terminal_view_id) {
        // return;
    }
    
    FColor margin_color = get_panel_margin_color(is_active_view ? UIHighlight_Active : UIHighlight_None);
    Rect_f32 region;
    {
        ARGB_Color margin_argb = fcolor_resolve(margin_color);
        ARGB_Color back_argb = fcolor_resolve(fcolor_id(defcolor_back));

        region = view_get_screen_rect(app, view_id);
        Rect_f32 inner_rect = region;
        inner_rect.x0 += 1.0f;
        inner_rect.y0 += 1.0f;
        inner_rect.x1 -= 1.0f;
        // inner_rect.y1 -= 1.0f;

        // Rect_f32 inner = rect_inner(region, width);
        draw_rectangle(app, inner_rect, 0.f, back_argb);
        // if (width > 0.f){
            draw_margin(app, region, inner_rect, margin_argb);
        // }
        // return(inner);
        region = inner_rect;
    }
    //  region = draw_background_and_margin(app, view_id, margin_color, fcolor_id(defcolor_back), 1.f);

    // Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
    Rect_f32 prev_clip = draw_set_clip(app, region);
    
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;
    
    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
        Rect_f32_Pair pair = layout_file_bar_on_bot(region, line_height);
        personal_draw_file_bar(app, view_id, buffer, face_id, pair.max);
        region = pair.min;
    }
    
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
    
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                  frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }
    
    // NOTE(allen): query bars
    region = default_draw_query_bars(app, region, view_id, face_id);
    
    // NOTE(allen): FPS hud
    // if (show_fps_hud){
    //     Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
    //     draw_fps_hud(app, frame_info, face_id, pair.max);
    //     region = pair.min;
    //     animate_in_n_milliseconds(app, 1000);
    // }
    
    // NOTE(allen): layout line numbers
    // b32 show_line_number_margins = def_get_config_b32(vars_save_string_lit("show_line_number_margins"));
    Rect_f32 line_number_rect = {};
    // if (show_line_number_margins){
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        line_number_rect.x0 += 10.0f;
        region = pair.max;
    // }
    
    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
    
    // NOTE(allen): draw line numbers
    // if (show_line_number_margins){
        draw_rectangle_fcolor(app, line_number_rect, 0.0f, fcolor_id(defcolor_cursor));
        draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);

    // }
    
    // NOTE(allen): draw the buffer
    personal_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
    
    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

function void
personal_whole_screen_render_caller(Application_Links *app, Frame_Info frame_info)
{
    // Rect_f32 region = global_get_screen_rectangle(app);
    // Vec2_f32 center = rect_center(region);
    
    // Face_ID face_id = get_face_id(app, 0);
    // Scratch_Block scratch(app);
    // FColor rect_color = fcolor_id(defcolor_back);

    // Rect_f32 status_region = Rf32(region.x0, region.y1 - 20.0f, region.x1, region.y1);
    // draw_rectangle_fcolor(app, status_region, 0.0f, rect_color);
    // personal_draw_status_bar(app, face_id, status_region);
}

Rect_f32 personal_view_get_screen_rect_type(Application_Links* app, View_ID view_id)
{
    // Rect_f32 global_region = global_get_screen_rectangle(app);
    Rect_f32 rect = default_view_get_screen_rect(app, view_id);
    // if (rect.y1 >= global_region.y1) {
    //     rect.y1 -= 20.0f;
    // }
    return rect;
}
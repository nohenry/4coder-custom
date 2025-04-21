#include "4coder_default_include.cpp"
#include "4coder_vim_script.cpp"
#include "4coder_terminal.cpp"

enum class Vim_Mode {
    Normal,
    Insert,
    Visual,
};

static struct {
    Vim_Mode mode; 
    i64 visual_mark;
    bool in_command;

    View_ID last_view;
    View_ID command_view;

    Command_Map_ID normal_map_id;
    Command_Map_ID insert_map_id;
    Command_Map_ID visual_map_id;
    Command_Map_ID common_map_id;
} global_vim_state;

void set_current_mapid(Application_Links* app, Command_Map_ID mapid)
{
    View_ID view = get_active_view( app, 0 );
    Buffer_ID buffer = view_get_buffer( app, view, 0 );
    Managed_Scope scope = buffer_get_managed_scope( app, buffer );
    Command_Map_ID* map_id_ptr = scope_attachment( app, scope, buffer_map_id, Command_Map_ID );
    *map_id_ptr = mapid;
}

static
void move_same_line_delta(Application_Links* app, i64 delta)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    cursor.col += delta;
    view_set_cursor_and_preferred_x(app, view, seek_line_col(cursor.line, cursor.col));
    no_mark_snap_to_cursor_if_shift(app, view);
}

void vim_update_visual(Application_Links* app)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);
    // isearch__update_highlight(app, view, { global_vim_state.visual_mark, cursor_pos + 1 });
    view_set_highlight_range(app, view, { global_vim_state.visual_mark, cursor_pos + 1 });
}

CUSTOM_COMMAND_SIG(escape_mode);

// { foobar   }

#pragma region Common

function i64
boundary_alpha_numeric_start(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos){
    static auto predicate = character_predicate_or(&character_predicate_alpha_numeric, &character_predicate_whitespace);
    return(boundary_predicate(app, buffer, side, direction, pos, &predicate));
}

function i64
boundary_not_alpha_numeric(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos){
    Character_Predicate not_alpha = character_predicate_not(&character_predicate_alpha_numeric);
    return(boundary_predicate(app, buffer, side, direction, pos, &not_alpha));
}

function i64
boundary_whitespace(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos){
    return(boundary_predicate(app, buffer, side, direction, pos, &character_predicate_whitespace));
}

function i64
boundary_whitespace_no_newline(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos){
    Character_Predicate predicate = character_predicate_whitespace;
    Character_Predicate nl = character_predicate_from_character('\n');
    nl = character_predicate_not(&nl);
    predicate = character_predicate_and(&predicate, &nl);
    return(boundary_predicate(app, buffer, side, direction, pos, &predicate));
}

function User_Input
get_next_input_with_modifer(Application_Links *app, Event_Property use_flags, Event_Property abort_flags){
    User_Input in = {};
    if (use_flags != 0){
        for (;;){
            in = get_next_input_raw(app);
            if (in.abort){
                break;
            }
            Event_Property event_flags = get_event_properties(&in.event);
            print_message(app, str8_lit("here"));
            if (
                in.event.key.code == KeyCode_Shift
                || in.event.key.code == KeyCode_Control
                || in.event.key.code == KeyCode_Alt
                || in.event.key.code == KeyCode_Command
            ) continue;
            if ((event_flags & abort_flags) != 0){
                in.abort = true;
                break;
            }
            if ((event_flags & use_flags) != 0){
                break;
            }
        }
    }
    return(in);
}

const Key_Code vim_pairs[][5] = {
    { KeyCode_LeftBracket , KeyCode_RightBracket , KeyCode_Shift, '{', '}' },
    { KeyCode_LeftBracket , KeyCode_RightBracket , 0            , '[', ']' },
    { KeyCode_9           , KeyCode_0            , KeyCode_Shift, '(', ')' },
    { KeyCode_Quote       , KeyCode_Quote        , KeyCode_Shift, '"', '"' },
    { KeyCode_Quote       , KeyCode_Quote        , 0            , '\'', '\'' },
};

bool vim_get_text_object(Application_Links* app, Arena* scratch, View_ID view, Buffer_ID buffer, bool inner, i64* start_pos, i64* end_pos)
{
    i64 cursor_pos = view_get_cursor_pos(app, view);
    User_Input input = get_next_input(app, EventProperty_AnyKey, EventProperty_Escape);
    if (input.abort) return false;
    while (input.event.key.code == KeyCode_Shift) {
        input = get_next_input(app, EventProperty_AnyKey, EventProperty_Escape);
        if (input.abort) return false;
    }
    switch (input.event.key.code) {
        case KeyCode_W: {
            auto fun_back = boundary_alpha_numeric_underscore;
            auto fun_forward = boundary_alpha_numeric_underscore;

            char on_char = buffer_get_char(app, buffer, cursor_pos);
            if (character_is_whitespace(on_char)) {
                if (inner) {
                    fun_back = fun_forward = boundary_whitespace_no_newline;
                } else {
                    fun_back = boundary_whitespace_no_newline;
                    fun_forward = boundary_non_whitespace;
                }
            } else {
                if (inner) {
                    fun_back = fun_forward = boundary_non_whitespace;
                } else {
                    fun_back = boundary_non_whitespace;
                    fun_forward = boundary_whitespace;
                }
            }

            *start_pos = scan(app, push_boundary_list(scratch, fun_back), buffer, Scan_Backward, cursor_pos + 1);
            *end_pos = scan(app, push_boundary_list(scratch, fun_forward), buffer, Scan_Forward, cursor_pos);
            break;
        }
        default:
            int found = -1;

            for (int i = 0; i < sizeof(vim_pairs)/sizeof(vim_pairs[0]); i++) {
                if (vim_pairs[i][2] != 0 && !has_modifier(&input, vim_pairs[i][2])) continue;

                if (input.event.key.code == vim_pairs[i][0] || input.event.key.code == vim_pairs[i][1]) {
                    found = i;
                    break;
                }
            }

            if (found < 0) return false;

            Character_Predicate backward_predicate = character_predicate_from_character((char)vim_pairs[found][3]);
            Character_Predicate forward_predicate = character_predicate_from_character((char)vim_pairs[found][4]);
            String_Match match_start = buffer_seek_character_class(app, buffer, &backward_predicate, Scan_Backward, cursor_pos + 1);
            String_Match match_end = buffer_seek_character_class(app, buffer, &forward_predicate, Scan_Forward, cursor_pos - 1);

            if (inner) {
                *start_pos = match_start.range.start + 1;
                *end_pos = match_end.range.end - 1;
            } else {
                *start_pos = match_start.range.start;
                *end_pos = match_end.range.end;
            }
            return true;
    }

    return true;
}


struct Key_Movement {
    Custom_Command_Function *func;
    Key_Code codes[4] = {};
    // Key_Code code1, code2 = 0;
};

CUSTOM_COMMAND_SIG(move_left_same_line)
{
    move_same_line_delta(app, -1);
}

CUSTOM_COMMAND_SIG(move_right_same_line)
{
    move_same_line_delta(app, 1);
}

CUSTOM_COMMAND_SIG(move_left_start_of_word)
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward, push_boundary_list(scratch, boundary_alpha_numeric));
}

CUSTOM_COMMAND_SIG(move_right_start_of_word)
{
    Scratch_Block scratch(app);

    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    i64 pos = scan(app, push_boundary_list(scratch, boundary_alpha_numeric), buffer, Scan_Forward, cursor_pos);
    pos = scan(app, push_boundary_list(scratch, boundary_not_alpha_numeric), buffer, Scan_Forward, cursor_pos);

    view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
    no_mark_snap_to_cursor_if_shift(app, view);
}

// CUSTOM_COMMAND_SIG(move_left_end_of_word)
// {
//     Scratch_Block scratch(app);
//     current_view_scan_move(app, Scan_Forward, push_boundary_list(scratch, boundary_alpha_numeric));
//     move_left(app);
// }

CUSTOM_COMMAND_SIG(move_right_end_of_word)
{
    Scratch_Block scratch(app);

    View_ID view = get_active_view(app, Access_ReadVisible);
    view_set_cursor_by_character_delta(app, view, 1);
    current_view_scan_move(app, Scan_Forward, push_boundary_list(scratch, boundary_alpha_numeric));
    view_set_cursor_by_character_delta(app, view, -1);
    no_mark_snap_to_cursor_if_shift(app, view);
}

CUSTOM_COMMAND_SIG(seek_beginning_of_line_non_whitesapce)
{
    Scratch_Block scratch(app);
    seek_pos_of_visual_line(app, Side_Min);

    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    if (character_is_whitespace(buffer_get_char(app, buffer, view_get_cursor_pos(app, view)))) {
        current_view_scan_move(app, Scan_Forward, push_boundary_list(scratch, boundary_whitespace));
    }
}

CUSTOM_COMMAND_SIG(vim_paste)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);

    Scratch_Block scratch(app);
    String_Const_u8 string = push_clipboard_index(scratch, 0, 0);

    if (global_vim_state.mode == Vim_Mode::Visual) {
        clipboard_post_buffer_range(app, 0, buffer, { global_vim_state.visual_mark, cursor_pos + 1 });

        buffer_replace_range(app, buffer, { global_vim_state.visual_mark, cursor_pos + 1 }, string);
        escape_mode(app);
    } else {
        buffer_replace_range(app, buffer, { cursor_pos + 1, cursor_pos + 1 }, string);
        move_same_line_delta(app, 1);
    }
}

CUSTOM_COMMAND_SIG(vim_paste_before)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);

    Scratch_Block scratch(app);
    String_Const_u8 string = push_clipboard_index(scratch, 0, 0);

    if (global_vim_state.mode == Vim_Mode::Visual) {
        buffer_replace_range(app, buffer, { global_vim_state.visual_mark, cursor_pos + 1 }, string);
        escape_mode(app);
    } else {
        buffer_replace_range(app, buffer, { cursor_pos, cursor_pos }, string);
    }
}

CUSTOM_COMMAND_SIG(escape_mode)
{
    if (global_vim_state.mode == Vim_Mode::Visual) {
        View_ID view = get_active_view(app, Access_ReadVisible);
        view_disable_highlight_range(app, view);
    }
    if (global_vim_state.mode == Vim_Mode::Insert) {
        move_same_line_delta(app, -1);
    }

    if (global_vim_state.in_command) {
        if (global_vim_state.mode == Vim_Mode::Normal) {
            global_vim_state.in_command = false;
            view_set_active(app, global_vim_state.last_view);
        }
    }

    global_vim_state.mode = Vim_Mode::Normal;
    set_current_mapid(app, global_vim_state.normal_map_id);
}

template<Custom_Command_Function Func>
CUSTOM_COMMAND_SIG(common_wrapper)
{
    Func(app);
    if (global_vim_state.mode == Vim_Mode::Visual) vim_update_visual(app);
}

static Key_Movement vim_movements[] = {
    { common_wrapper<move_down>                            , { KeyCode_J                      } },
    { common_wrapper<move_up>                              , { KeyCode_K                      } },
    { common_wrapper<move_left_same_line>                  , { KeyCode_H                      } },
    { common_wrapper<move_right_same_line>                 , { KeyCode_L                      } },
    { common_wrapper<move_right_start_of_word>             , { KeyCode_W                      } },
    { common_wrapper<move_left_start_of_word>              , { KeyCode_B                      } },
    { common_wrapper<move_right_end_of_word>               , { KeyCode_E                      } },
    { common_wrapper<move_right_whitespace_boundary>       , { KeyCode_W      , KeyCode_Shift } },
    { common_wrapper<move_left_whitespace_boundary>        , { KeyCode_B      , KeyCode_Shift } },
    { common_wrapper<seek_beginning_of_line>               , { KeyCode_0                      } },
    { common_wrapper<seek_beginning_of_line_non_whitesapce>, { KeyCode_Minus  , KeyCode_Shift } },
    { common_wrapper<seek_end_of_line>                     , { KeyCode_4      , KeyCode_Shift } },
};

#pragma endregion Common

#pragma region Visual

CUSTOM_COMMAND_SIG(enter_visual)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    global_vim_state.visual_mark = view_get_cursor_pos(app, view);

    global_vim_state.mode = Vim_Mode::Visual;
    set_current_mapid(app, global_vim_state.visual_map_id);
}

CUSTOM_COMMAND_SIG(copy_highlight)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);
    clipboard_post_buffer_range(app, 0, buffer, { global_vim_state.visual_mark, cursor_pos + 1 });

    escape_mode(app);
}

CUSTOM_COMMAND_SIG(delete_highlight)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);
    clipboard_post_buffer_range(app, 0, buffer, { global_vim_state.visual_mark, cursor_pos + 1 });

    buffer_replace_range(app, buffer, { global_vim_state.visual_mark, cursor_pos + 1 }, string_u8_empty);

    escape_mode(app);
}

CUSTOM_COMMAND_SIG(vim_visual_text_object)
{
    Scratch_Block scratch(app);
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 start_pos, end_pos;
    if (!vim_get_text_object(app, scratch, view, buffer, false, &start_pos, &end_pos)) {
        return;
    }

    i64 cursor_pos = view_get_cursor_pos(app, view);
    global_vim_state.visual_mark = Min(global_vim_state.visual_mark, start_pos);
    cursor_pos = Max(cursor_pos, end_pos - 1);

    view_set_cursor(app, view, seek_pos(cursor_pos));
    vim_update_visual(app);
}

CUSTOM_COMMAND_SIG(vim_visual_text_object_inner)
{
    Scratch_Block scratch(app);
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 start_pos, end_pos;
    if (!vim_get_text_object(app, scratch, view, buffer, true, &start_pos, &end_pos)) {
        return;
    }

    i64 cursor_pos = view_get_cursor_pos(app, view);
    global_vim_state.visual_mark = Min(global_vim_state.visual_mark, start_pos);
    cursor_pos = Max(cursor_pos, end_pos - 1);
    // { foobar } ff

    view_set_cursor(app, view, seek_pos(cursor_pos));
    vim_update_visual(app);
}

#pragma endregion Visual

#pragma region Normal

CUSTOM_COMMAND_SIG(insert_mode)
{
    global_vim_state.mode = Vim_Mode::Insert;
    set_current_mapid(app, global_vim_state.insert_map_id);
}

CUSTOM_COMMAND_SIG(insert_mode_after)
{
    global_vim_state.mode = Vim_Mode::Insert;
    move_same_line_delta(app, 1);
    set_current_mapid(app, global_vim_state.insert_map_id);
}

CUSTOM_COMMAND_SIG(insert_start_of_line)
{
    global_vim_state.mode = Vim_Mode::Insert;
    seek_beginning_of_line(app);
    set_current_mapid(app, global_vim_state.insert_map_id);
}

CUSTOM_COMMAND_SIG(insert_end_of_line)
{
    global_vim_state.mode = Vim_Mode::Insert;
    seek_end_of_line(app);
    set_current_mapid(app, global_vim_state.insert_map_id);
}

CUSTOM_COMMAND_SIG(insert_above)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    global_vim_state.mode = Vim_Mode::Insert;
    seek_beginning_of_line(app);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    buffer_replace_range(app, buffer, { cursor_pos, cursor_pos }, str8_lit("\n"));
    view_set_cursor(app, view, seek_pos(cursor_pos - 1));

    set_current_mapid(app, global_vim_state.insert_map_id);
}

CUSTOM_COMMAND_SIG(insert_under)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    global_vim_state.mode = Vim_Mode::Insert;
    seek_end_of_textual_line(app);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    buffer_replace_range(app, buffer, { cursor_pos, cursor_pos }, str8_lit("\n"));
    view_set_cursor(app, view, seek_pos(cursor_pos + 1));

    set_current_mapid(app, global_vim_state.insert_map_id);
}

inline Key_Movement* get_key_movement(User_Input* event) {
    for (int i = 0; i < sizeof(vim_movements)/sizeof(vim_movements[0]); i++) {
        if (vim_movements[i].codes[0] == event->event.key.code) {
            Key_Code* code = vim_movements[i].codes + 1;

            while (*code) {
                if (!has_modifier(event, *code)) {
                    goto DONE_ITERATION;
                }
            }

            return &vim_movements[i];
        }
DONE_ITERATION: (void)0;
    }
    return nullptr;
}

CUSTOM_COMMAND_SIG(copy_movement)
{
    View_ID view = get_active_view(app, Access_Read);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Read);
    i64 cursor_pos = view_get_cursor_pos(app, view);

    User_Input input = get_next_input(app, EventProperty_AnyKey, EventProperty_Escape);
    if (input.abort) return;
    
    Key_Movement* movement = get_key_movement(&input);
    if (!movement) return;
    movement->func(app);

    i64 current_cursor_pos = view_get_cursor_pos(app, view);
    clipboard_post_buffer_range(app, 0, buffer, { cursor_pos, current_cursor_pos + 1 });

    view_set_cursor(app, view, seek_pos(cursor_pos));
}

CUSTOM_COMMAND_SIG(delete_movement)
{
    Scratch_Block scratch(app);
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    i64 cursor_pos = view_get_cursor_pos(app, view);

    User_Input input = get_next_input_with_modifer(app, EventProperty_AnyKey, EventProperty_Escape);
    if (input.abort) return;

    if (input.event.key.code == KeyCode_I || input.event.key.code == KeyCode_A) {
        i64 start_pos, end_pos;
        if (!vim_get_text_object(app, scratch, view, buffer, input.event.key.code == KeyCode_I, &start_pos, &end_pos)) {
            return;
        }

        clipboard_post_buffer_range(app, 0, buffer, { start_pos, end_pos });
        buffer_replace_range(app, buffer, { start_pos, end_pos }, string_u8_empty);

        view_set_cursor(app, view, seek_pos(start_pos));
    } else {
        // while (input.event.key.code == KeyCode_Shift) {
        //     input = get_next_input(app, EventProperty_AnyKey, EventProperty_Escape);
        //     if (input.abort) return;
        // }

        Key_Movement* movement = get_key_movement(&input);
        if (!movement) return;
        movement->func(app);

        i64 current_cursor_pos = view_get_cursor_pos(app, view);
        clipboard_post_buffer_range(app, 0, buffer, { cursor_pos, current_cursor_pos + 1 });
        buffer_replace_range(app, buffer, { cursor_pos, current_cursor_pos + 1 }, string_u8_empty);

        view_set_cursor(app, view, seek_pos(cursor_pos));
    }
}

CUSTOM_COMMAND_SIG(change_movement)
{
    Scratch_Block scratch(app);
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    i64 cursor_pos = view_get_cursor_pos(app, view);

    User_Input input = get_next_input(app, EventProperty_AnyKey, EventProperty_Escape);
    if (input.abort) return;

    if (input.event.key.code == KeyCode_I || input.event.key.code == KeyCode_A) {
        i64 start_pos, end_pos;
        if (!vim_get_text_object(app, scratch, view, buffer, input.event.key.code == KeyCode_I, &start_pos, &end_pos)) {
            return;
        }

        clipboard_post_buffer_range(app, 0, buffer, { start_pos, end_pos });
        buffer_replace_range(app, buffer, { start_pos, end_pos }, string_u8_empty);

        view_set_cursor(app, view, seek_pos(start_pos));
    } else {
        while (input.event.key.code == KeyCode_Shift) {
            input = get_next_input(app, EventProperty_AnyKey, EventProperty_Escape);
            if (input.abort) return;
        }

        Key_Movement* movement = get_key_movement(&input);
        if (!movement) return;
        movement->func(app);

        i64 current_cursor_pos = view_get_cursor_pos(app, view);
        clipboard_post_buffer_range(app, 0, buffer, { cursor_pos, current_cursor_pos + 1 });
        buffer_replace_range(app, buffer, { cursor_pos, current_cursor_pos + 1 }, string_u8_empty);

        view_set_cursor(app, view, seek_pos(cursor_pos));
    }

    insert_mode(app);
}

#pragma endregion Normal

CUSTOM_COMMAND_SIG(command_mode)
{
    global_vim_state.in_command = true;
    global_vim_state.last_view = get_active_view(app, Access_Visible);

    Buffer_ID command_buffer = view_get_buffer(app, global_vim_state.command_view, Access_ReadWrite);
    Range_i64 command_buffer_range = buffer_range(app, command_buffer);
    buffer_replace_range(app, command_buffer, command_buffer_range, string_u8_empty);
    
    view_set_active(app, global_vim_state.command_view);
    insert_mode(app);
}

CUSTOM_COMMAND_SIG(vim_write_text_input)
{
    if (global_vim_state.in_command) {

    } else {
        write_text_input(app);
    }
}

CUSTOM_COMMAND_SIG(vim_type_enter)
{
    Scratch_Block scratch(app);
    if (global_vim_state.in_command) {
        Buffer_ID buffer = view_get_buffer(app, global_vim_state.command_view, Access_Read);
        String_Const_u8 contents = push_whole_buffer(app, scratch, buffer);
        vim_script_parse_command(app, global_vim_state.last_view, contents);

        global_vim_state.in_command = false;
        view_set_active(app, global_vim_state.last_view);

        global_vim_state.mode = Vim_Mode::Normal;
        set_current_mapid(app, global_vim_state.normal_map_id);
    } else {
        write_text(app, str8_lit("\n"));
    }
}

CUSTOM_COMMAND_SIG(vim_window)
{
    User_Input in = get_next_input(app, EventProperty_AnyKey, EventProperty_Escape);
    if (in.abort) return;
    escape_mode(app);

    View_ID view_id = get_active_view(app, Access_Always);
    View_ID last_view_id;
    Buffer_ID buffer_id = view_get_buffer(app, view_id, Access_Always);
    for (;;) {
        switch (in.event.key.code) {
            case KeyCode_H:
                view_id = get_view_next(app, view_id, Access_Always);
                break;
            case KeyCode_L:
                view_id = get_view_prev(app, view_id, Access_Always);
                break;

            case KeyCode_V:
                view_id = open_view(app, view_id, ViewSplit_Right);
                view_set_buffer(app, view_id, buffer_id, 0);
                break;

            case KeyCode_N:
                view_id = open_view(app, view_id, ViewSplit_Bottom);
                view_set_buffer(app, view_id, buffer_id, 0);
                break;

            case KeyCode_Q:
                last_view_id = get_view_next(app, view_id, Access_Always);
                view_close(app, view_id);
                view_id = last_view_id;
                break;
            
            default: return;
        }

        if (view_id == 0 || !view_get_is_passive(app, view_id) && view_id != global_vim_state.command_view) break;
    }

    if (view_id == 0 || view_get_is_passive(app, view_id) || view_id == global_vim_state.command_view) return;
    view_set_active(app, view_id);
    if (view_id == terminal_view_id) {
        insert_mode(app);
        // set_current_mapid(app, global_vim_state.insert_map_id);
    } else {
        set_current_mapid(app, global_vim_state.normal_map_id);
    }
}

#pragma region Setup

BUFFER_HOOK_SIG(vim_begin_buffer_handler)
{
    default_begin_buffer(app, buffer_id);
    set_current_mapid(app, global_vim_state.normal_map_id);

    return 0;
}

CUSTOM_COMMAND_SIG(vim_startup)
{
    default_startup(app);

    View_ID view = get_active_view(app, Access_Visible);

    Buffer_ID buffer = create_buffer(app, string_u8_empty, BufferCreate_NeverAttachToFile);
    // (void)buffer;
    global_vim_state.command_view = open_view(app, get_active_view(app, Access_Always), ViewSplit_Bottom);
    view_set_buffer(app, global_vim_state.command_view, buffer, 0);
    // panel_
    // view_get_panel()
    // panel_set_split()



    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;

    view_set_split(app, global_vim_state.command_view, ViewSplitKind_FixedPixels, line_height + 2.0f);

    view_set_active(app, view);



    terminal_setup(app);
    insert_mode(app);

    // buffer_set_setting();
}

void vim_setup(Application_Links* app, String_ID global_id, String_ID file_id, String_ID code_id) {
    global_vim_state.normal_map_id = vars_save_string_lit("keys_vim_normal");
    global_vim_state.common_map_id = vars_save_string_lit("keys_vim_common");
    global_vim_state.visual_map_id = vars_save_string_lit("keys_vim_visual");
    global_vim_state.insert_map_id = vars_save_string_lit("keys_vim_insert");
    global_vim_state.in_command = false;

    MappingScope();
    SelectMapping(&framework_mapping);

    SelectMap(file_id);
    // BindTextInput(vim_write_text_input);

    SelectMap(global_vim_state.insert_map_id);
    ParentMap(file_id);
    Bind(backspace_char, KeyCode_Backspace);
    Bind(escape_mode, KeyCode_Escape);

    Bind(vim_type_enter, KeyCode_Return);

    SelectMap(global_vim_state.common_map_id);
    ParentMap(global_id);
    for (int i = 0; i < sizeof(vim_movements)/sizeof(vim_movements[0]); i++) {
        if (vim_movements[i].codes[1]) {
            Bind(vim_movements[i].func, vim_movements[i].codes[0], vim_movements[i].codes[1]);
        } else {
            Bind(vim_movements[i].func, vim_movements[i].codes[0]);
        }
    }

    Bind(vim_paste, KeyCode_P);
    Bind(vim_paste_before, KeyCode_P, KeyCode_Shift);

    Bind(undo, KeyCode_U);
    Bind(command_mode, KeyCode_Semicolon);

    SelectMap(global_vim_state.normal_map_id);
    ParentMap(global_vim_state.common_map_id);
    Bind(insert_mode, KeyCode_I);
    Bind(insert_start_of_line, KeyCode_I, KeyCode_Shift);
    Bind(insert_mode_after, KeyCode_A);
    Bind(insert_end_of_line, KeyCode_A, KeyCode_Shift);
    Bind(insert_above, KeyCode_O, KeyCode_Shift);
    Bind(insert_under, KeyCode_O);
    Bind(enter_visual, KeyCode_V);

    Bind(copy_movement, KeyCode_Y);
    Bind(delete_movement, KeyCode_D);
    Bind(change_movement, KeyCode_C);
    // { foo bar}

    Bind(change_active_panel, KeyCode_N);
    Bind(escape_mode, KeyCode_Escape);

    SelectMap(global_vim_state.visual_map_id);
    ParentMap(global_vim_state.common_map_id);
    Bind(copy_highlight, KeyCode_Y);
    Bind(delete_highlight, KeyCode_D);
    Bind(vim_visual_text_object, KeyCode_A);
    Bind(vim_visual_text_object_inner, KeyCode_I);
    Bind(escape_mode, KeyCode_Escape);


    SelectMap(global_id);
    Bind(vim_window, KeyCode_W, KeyCode_Control);
    BindCore(vim_startup, CoreCode_Startup);

    set_custom_hook(app, HookID_BeginBuffer, vim_begin_buffer_handler);

    // set_current_mapid(app, global_vim_state.normal_map_id);
}

// hello my name    is oliver
#pragma endregion Setup

#include "4coder_default_include.cpp"

void vim_script_parse_command(Application_Links* app, View_ID view, String_Const_u8 raw_input)
{
    if (string_match(raw_input, str8_lit("wq"))) {
        save(app);
        exit_4coder(app);
    } else if (string_match(raw_input, str8_lit("q"))) {
        exit_4coder(app);
    }
}
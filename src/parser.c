/*
 * The grammar rules, in plain-English, are:
 *
 *   1. Section-titles must be followed by an open-brace. The lexer guarantees
 *      that section-title keywords may only exist at the root level of the
 *      source document and that every section-title is followed by an L-brace.
 *   2. Sections may only contain parameters and their paired values. The lexer
 *      guarantees that both L-braces and section-titles are illegal within the
 *      scope of a section.
 *   3. Parameters must be paired with one or more values. The lexer *could*
 *      guarantee this, but it does not to open the possibility of parameters
 *      which accept no argument.
 *   4. Parameters must match their section. For simplicity, the lexer does not
 *      guarantee this and passes the buck to the parser. This would require
 *      some amount of lookback conditionals, which would clutter the logic.
 *   5. Parameters must match a value-type. Similar to the above, the lexer does
 *      not implement any lookback conditionals, and so this responsibility is
 *      deferred to the parser.
 *
 * So, there are really only three conditions that the parser must evaluate, all
 * of which are tied to parameters:
 *
 *   1. Every parameter has at least one value.
 *   2. Every parameter's values match its type bounds.
 *   3. The scope of valid parameters is contingent on the section.
 *
 * Rule 1 is simple to verify with some arithmetic:
 *
 *   error = E_missing_value * (
 *      type == T_section_end
 *          || type >= T_param_title
 *          || type <= T_direc_addfile
 *   )
 *
 * Rule 2 requires some look-ahead, which means our grammar is functionally an
 * LL(k) grammar. This is safe to do because the lexer guarantees that the final
 * token in the document _must_ be an R-brace for the final section.
 *
 * Rule 3 requires an additional layer of look-back, this time to the section.
 * This could also be implemented instead as each section having its own inner
 * switch-table of the state machine, where any unrecognized parameter or stray
 * value token evaluates to an error.
 *
 * We can structure all of this logic, then, as a simple state machine, where a
 * top-level state (S_root) looks for and consumes section-title tokens, each of
 * which has their own inner finite-state machine. A section's FSM permits only
 * those tokens which it recognizes as its valid parameters. An individual
 * parameter-state will then look-ahead to the next token and verify that its
 * value matches the expected type. For example, S_properties would accept:
 *
 *   - T_param_title    -> take 1 string value and transition to S_properties
 *   - T_param_romtype  -> take either MROM or PROM and transition to S_properties
 *   - T_param_revision -> take 1 number value and transition to S_properties
 *   - etc.
 *
 * The next token not matching the expected type, as described above, represents
 * an error and will immediately terminate the main-loop.
 *
 * This isn't so nice for some sections which share a set of valid parameters
 * yet point to different destination structures, e.g., ARM7 and ARM9. We could
 * simply duplicate the FSM, of course, but this creates a maintenance problem:
 * updates to either section's FSM would need to be mirrored against the other.
 * An alternative might be to split the full FSM into a network of automata,
 * with each *type* of section having a distinct state-transition table that is
 * given control by the main loop and performs its own inner-loop. Then,
 * mirrored sections could maintain some inner-state of their own to know what
 * the destination structure is. This, however, seems cumbersome, and I don't
 * expect us to update the structure that often for the ARM sections, if ever.
 * The cost of additional bytes seems worth simplifying the actual code.
 */

#include "parser.h"

#include <stdlib.h>
#include <string.h>

#include "lexer.h"

typedef enum {
    S_error = 0,

    S_root,
    S_properties,
    S_arm9,
    S_arm7,
    S_layout,

    S_param_title,
    S_param_serial,
    S_param_makercode,
    S_param_revision,
    S_param_romtype,
    S_param_romcapacity,
    S_param_padtoend,
    S_param_headertemplate,
    S_param_bootmenubanner,

    S_param_arm9_codebinary,
    S_param_arm9_definitions,
    S_param_arm9_overlaytable,

    S_param_arm7_codebinary,
    S_param_arm7_definitions,
    S_param_arm7_overlaytable,

    S_param_settargetpath,
    S_param_setsourcepath,
    S_param_addfile,

    E_none,                  // only for the semantics
    E_invalid_section_param, // emitted by the state machine
    E_missing_value,         // all others emitted by the state-processor
    E_expected_boolean,
    E_expected_number,
    E_expected_string,
    E_expected_filepath, // these technically also support strings; impl detail
    E_expected_romtype,
    E_reallocation_error,
} State;

#define NUM_SECTION_STATES (S_param_title - S_error)

// Some initial estimates on a "reasonable" ROM filesystem size. This only
// considers filesystem entries; overlays do not count toward these values, even
// though they are given filesystem IDs.
//
// These numbers are based on the counts present in Pokemon Platinum (U).
#define INITIAL_DIRLIST_CAPACITY  128
#define INITIAL_FILELIST_CAPACITY 512
#define LEN_FILEPATH              192

// State-transition table. Individual processing states accept a subset of valid
// tokens and report a state back which signals the error-type or a next-state.
// The latter of these points to a switch-arm that consumes values as needed.
static const u8 transition[NUM_SECTION_STATES][NUM_TOKEN_TYPES];

#define take_string(dest, max_len)                                            \
    {                                                                         \
        tmp = min((u32)p_value->len, max_len);                                \
        strncpy(dest, source + p_value->begin, tmp);                          \
        error = p_value->type != T_value_string ? E_expected_string : E_none; \
    }
#define take_filepath(dest)                                                                                            \
    {                                                                                                                  \
        tmp = min((u32)p_value->len, LEN_FILEPATH);                                                                    \
        memset(dest, 0, LEN_FILEPATH);                                                                                 \
        strncpy(dest, source + p_value->begin, tmp);                                                                   \
        error = (p_value->type != T_value_filepath && p_value->type != T_value_string) ? E_expected_filepath : E_none; \
    }
#define take_number(dest)                                                     \
    {                                                                         \
        dest = p_value->n_value;                                              \
        error = p_value->type != T_value_number ? E_expected_number : E_none; \
    }
#define take_boolean(dest)                                                                                       \
    {                                                                                                            \
        dest = p_value->b_value;                                                                                 \
        error = (p_value->type != T_value_true && p_value->type != T_value_false) ? E_expected_boolean : E_none; \
    }
#define take_romtype(dest)                                                                                      \
    {                                                                                                           \
        dest = ((p_value->type == T_value_MROM) * 0x051E) + ((p_value->type == T_value_PROM) * 0x0D7E);         \
        error = (p_value->type != T_value_MROM && p_value->type != T_value_PROM) ? E_expected_romtype : E_none; \
    }

static State add_file(ROMSpec *spec, char *target_root, char *source_root, char *filepath);

static inline void alloc_arm_paths(ARM *arm)
{
    arm->code_binary_fpath = calloc(LEN_FILEPATH, 1);
    arm->definitions_fpath = calloc(LEN_FILEPATH, 1);
    arm->overlay_table_fpath = calloc(LEN_FILEPATH, 1);
}

ParseResult parse(Token *tokens, int num_tokens, const char *source)
{
    // Make some assumptions for initial capacities and reallocate them as
    // needed. This will be a little slow for large-count filesystems, but most
    // builds should correctly predict the branch and skip the reallocation.
    ROMSpec *spec = calloc(1, sizeof(ROMSpec));
    spec->files = malloc(sizeof(File) * INITIAL_FILELIST_CAPACITY);
    spec->cap_files = INITIAL_FILELIST_CAPACITY;
    spec->properties.header_fpath = calloc(LEN_FILEPATH, 1);
    spec->properties.banner_fpath = calloc(LEN_FILEPATH, 1);
    alloc_arm_paths(&spec->arm9);
    alloc_arm_paths(&spec->arm7);

    State state = S_root;
    Token *p_token, *p_value;
    State error = E_none;
    int tmp;

    char source_root[LEN_FILEPATH] = {'.', '/', '\0'};
    char target_root[LEN_FILEPATH] = {'/', '\0'};
    char filepath[LEN_FILEPATH];

    for (int i = 0; i < num_tokens && error == E_none; i++) {
        do {
            p_token = &tokens[i++];
            state = transition[state][p_token->type];
        } while (i < num_tokens && state >= S_root && state <= S_layout);

        if (i == num_tokens) {
            break;
        }

        p_value = &tokens[i];
        if (p_value->type < T_value_PROM) {
            // Sanity check for a missing value.
            error = E_missing_value;
            tmp = i - 1;
            break;
        }

        switch (state) {
        case S_param_title:
            take_string(spec->properties.title, LEN_TITLE);
            state = S_properties;
            break;

        case S_param_serial:
            take_string(spec->properties.serial, LEN_SERIAL);
            state = S_properties;
            break;

        case S_param_makercode:
            take_string(spec->properties.maker, LEN_MAKER);
            state = S_properties;
            break;

        case S_param_revision:
            take_number(spec->properties.revision);
            state = S_properties;
            break;

        case S_param_romtype:
            take_romtype(spec->properties.rom_type);
            state = S_properties;
            break;

        case S_param_romcapacity:
            take_number(spec->properties.capacity);
            state = S_properties;
            break;

        case S_param_padtoend:
            take_boolean(spec->properties.pad_to_end);
            state = S_properties;
            break;

        case S_param_headertemplate:
            take_filepath(spec->properties.header_fpath);
            state = S_properties;
            break;

        case S_param_bootmenubanner:
            take_filepath(spec->properties.banner_fpath);
            state = S_properties;
            break;

        case S_param_arm9_codebinary:
            take_filepath(spec->arm9.code_binary_fpath);
            state = S_arm9;
            break;

        case S_param_arm9_definitions:
            take_filepath(spec->arm9.definitions_fpath);
            state = S_arm9;
            break;

        case S_param_arm9_overlaytable:
            take_filepath(spec->arm9.overlay_table_fpath);
            state = S_arm9;
            break;

        case S_param_arm7_codebinary:
            take_filepath(spec->arm7.code_binary_fpath);
            state = S_arm7;
            break;

        case S_param_arm7_definitions:
            take_filepath(spec->arm7.definitions_fpath);
            state = S_arm7;
            break;

        case S_param_arm7_overlaytable:
            take_filepath(spec->arm7.overlay_table_fpath);
            state = S_arm7;
            break;

        case S_param_settargetpath:
            take_filepath(target_root);
            state = S_layout;
            break;

        case S_param_setsourcepath:
            take_filepath(source_root);
            state = S_layout;
            break;

        case S_param_addfile:
            // AddFile is unique in that it supports one to many values
            do {
                take_filepath(filepath);
                error = add_file(spec, target_root, source_root, filepath);
                p_value = &tokens[++i];
            } while (p_value->type == T_value_string || p_value->type == T_value_filepath);

            i--; // back up once so that the next token is correctly processed
            state = S_layout;
            break;

        case E_invalid_section_param:
            // The FSM declared the parameter-token to be invalid for the
            // current section. Destroy the data structure and bail.
            error = E_invalid_section_param;
            dspec(spec);
            break;

        default: // All other states should be impossible to reach.
            abort();
        }
    }

    if (error > E_none) {
        return (ParseResult){
            .ok = false,
            .err_type = error,
            .err_idx = tmp,
        };
    }

    return (ParseResult){
        .ok = true,
        .spec = spec,
    };
}

static inline char *join_paths(const char *restrict parent, const char *restrict child, u32 len_parent, u32 len_child)
{
    len_parent = (len_parent != 1) * len_parent; // If the parent is *just* root, then treat it as a zero-length string

    char *path = malloc(len_parent + len_child + 2); // 1 separator + null-term
    strcpy(path, parent);
    strcpy(path + len_parent + 1, child);
    path[len_parent] = '/';
    path[len_parent + len_child + 1] = '\0';

    return path;
}

static State add_file(ROMSpec *spec, char *target_root, char *source_root, char *filepath)
{
    if (spec->len_files == spec->cap_files) {
        spec->cap_files *= 2;
        File *files = realloc(spec->files, sizeof(File) * spec->cap_files);
        if (!files) {
            dspec(spec);
            return E_reallocation_error;
        }

        spec->files = files;
    }

    u32 len_tr = strlen(target_root);
    u32 len_sr = strlen(source_root);
    u32 len_fp = strlen(filepath);

    spec->files[spec->len_files] = (File){
        .target_path = join_paths(target_root, filepath, len_tr, len_fp),
        .source_path = join_paths(source_root, filepath, len_sr, len_fp),
        .filesys_id = 0,
        .is_dir = 0,
        .packing_id = spec->len_files, // used later to map sorted-files back to the packing-order
    };

    spec->len_files++;
    return E_none;
}

void dspec(ROMSpec *spec)
{
    free(spec->properties.header_fpath);
    free(spec->properties.banner_fpath);
    free(spec->arm9.code_binary_fpath);
    free(spec->arm9.definitions_fpath);
    free(spec->arm9.overlay_table_fpath);
    free(spec->arm7.code_binary_fpath);
    free(spec->arm7.definitions_fpath);
    free(spec->arm7.overlay_table_fpath);

    for (u32 i = 0; i < spec->len_files; i++) {
        free(spec->files[i].target_path);
        free(spec->files[i].source_path);
    }

    free(spec->files);
    free(spec);
}

static const u8 transition[NUM_SECTION_STATES][NUM_TOKEN_TYPES] = {
    [S_error] = {0}, // Cannot escape the error state

    // The lexer guarantees that the first token in any given file is a title,
    // and that the first token after any given rbrace-token is a title. Thus,
    // we can keep the table declaration here small and lean on S_error.
    [S_root] = {
        [T_title_properties] = S_properties,
        [T_title_arm9] = S_arm9,
        [T_title_arm7] = S_arm7,
        [T_title_layout] = S_layout,
    },

    // The lexer guarantees that, within any given section, we will only have
    // parameter-tokens, value-tokens, and the brace-tokens that encase it.
    [S_properties] = {
        // The lbrace-token keeps us in this state; the token-iterator will
        // swallow it.
        [T_section_begin] = S_properties,

        // The rbrace-token takes us back to the top-level.
        [T_section_end] = S_root,

        // These are valid parameters for this section. The token-iterator will
        // break when any of them are encountered and defer control to the
        // switch-arm responsible for processing them.
        [T_param_title] = S_param_title,
        [T_param_serial] = S_param_serial,
        [T_param_makercode] = S_param_makercode,
        [T_param_revision] = S_param_revision,
        [T_param_romtype] = S_param_romtype,
        [T_param_romcapacity] = S_param_romcapacity,
        [T_param_padtoend] = S_param_padtoend,
        [T_param_headertemplate] = S_param_headertemplate,
        [T_param_bootmenubanner] = S_param_bootmenubanner,

        // These are not. We have a nicer error message for this.
        [T_param_codebinary] = E_invalid_section_param,
        [T_param_definitions] = E_invalid_section_param,
        [T_param_overlaytable] = E_invalid_section_param,
        [T_direc_settargetpath] = E_invalid_section_param,
        [T_direc_setsourcepath] = E_invalid_section_param,
        [T_direc_addfile] = E_invalid_section_param,
    },

    [S_arm9] = {
        [T_section_begin] = S_arm9,

        [T_section_end] = S_root,

        [T_param_codebinary] = S_param_arm9_codebinary,
        [T_param_definitions] = S_param_arm9_definitions,
        [T_param_overlaytable] = S_param_arm9_overlaytable,

        [T_param_title] = E_invalid_section_param,
        [T_param_serial] = E_invalid_section_param,
        [T_param_makercode] = E_invalid_section_param,
        [T_param_revision] = E_invalid_section_param,
        [T_param_romtype] = E_invalid_section_param,
        [T_param_romcapacity] = E_invalid_section_param,
        [T_param_padtoend] = E_invalid_section_param,
        [T_direc_settargetpath] = E_invalid_section_param,
        [T_direc_setsourcepath] = E_invalid_section_param,
        [T_direc_addfile] = E_invalid_section_param,
    },

    [S_arm7] = {
        [T_section_begin] = S_arm7,

        [T_section_end] = S_root,

        [T_param_codebinary] = S_param_arm7_codebinary,
        [T_param_definitions] = S_param_arm7_definitions,
        [T_param_overlaytable] = S_param_arm7_overlaytable,

        [T_param_title] = E_invalid_section_param,
        [T_param_serial] = E_invalid_section_param,
        [T_param_makercode] = E_invalid_section_param,
        [T_param_revision] = E_invalid_section_param,
        [T_param_romtype] = E_invalid_section_param,
        [T_param_romcapacity] = E_invalid_section_param,
        [T_param_padtoend] = E_invalid_section_param,
        [T_direc_settargetpath] = E_invalid_section_param,
        [T_direc_setsourcepath] = E_invalid_section_param,
        [T_direc_addfile] = E_invalid_section_param,
    },

    [S_layout] = {
        [T_section_begin] = S_layout,

        [T_section_end] = S_root,

        [T_direc_settargetpath] = S_param_settargetpath,
        [T_direc_setsourcepath] = S_param_setsourcepath,
        [T_direc_addfile] = S_param_addfile,

        [T_param_title] = E_invalid_section_param,
        [T_param_serial] = E_invalid_section_param,
        [T_param_makercode] = E_invalid_section_param,
        [T_param_revision] = E_invalid_section_param,
        [T_param_romtype] = E_invalid_section_param,
        [T_param_romcapacity] = E_invalid_section_param,
        [T_param_padtoend] = E_invalid_section_param,
        [T_param_codebinary] = E_invalid_section_param,
        [T_param_definitions] = E_invalid_section_param,
        [T_param_overlaytable] = E_invalid_section_param,
    },
};

const char *parse_error_messages[] = {
    [E_invalid_section_param] = "invalid section-parameter",
    [E_missing_value] = "missing value for parameter",
    [E_expected_boolean] = "expected boolean-type value",
    [E_expected_number] = "expected number-type value",
    [E_expected_string] = "expected string-type value",
    [E_expected_filepath] = "expected filepath- or string-type value",
    [E_expected_romtype] = "expected romtype-type value",
    [E_reallocation_error] = "fatal error reallocating files",
};

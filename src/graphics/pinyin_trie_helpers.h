#pragma once
#include "pinyin_trie_static.h"
#include <climits>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

std::string lastPinyin = "";
std::vector<std::string> lastresult = {};

// Platform-aware accessors for WORDS_DATA, WORDS_OFFSETS, WORDS_WEIGHT
// On AVR, WORDS_DATA/WORDS_OFFSETS/WORDS_WEIGHT are in PROGMEM and must be read via pgm_read_*.
#if defined(ARCH_ESP32)
static inline void fill_word_buf_from_progmem(uint32_t off, char *buf, size_t buflen)
{
    const unsigned char *p = WORDS_DATA + off;
    size_t i = 0;
    while (i + 1 < buflen) {
        unsigned char c = pgm_read_byte(p + i);
        buf[i] = (char)c;
        if (c == 0)
            break;
        i++;
    }
    buf[buflen - 1] = 0;
}
static inline const char *get_word_ptr_buf(uint32_t idx, char *buf, size_t buflen)
{
    uint32_t off = (uint32_t)pgm_read_dword(&(WORDS_OFFSETS[idx]));
    fill_word_buf_from_progmem(off, buf, buflen);
    return buf;
}
static inline int32_t get_word_weight(uint32_t idx)
{
    return (int32_t)pgm_read_dword(&(WORDS_WEIGHT[idx]));
}
#else
static inline const char *get_word_ptr_buf(uint32_t idx, char *buf, size_t buflen)
{
    uint32_t off = WORDS_OFFSETS[idx];
    const unsigned char *p = WORDS_DATA + off;
    size_t i = 0;
    while (i + 1 < buflen) {
        unsigned char c = p[i];
        buf[i] = (char)c;
        if (c == 0)
            break;
        i++;
    }
    buf[buflen - 1] = 0;
    return buf;
}
static inline int32_t get_word_weight(uint32_t idx)
{
    return WORDS_WEIGHT[idx];
}
#endif

// Accessor for WORD_CHAR_COUNT (number of characters in stored word)
#if defined(ARCH_ESP32)
static inline uint8_t get_word_char_count(uint32_t idx)
{
    return (uint8_t)pgm_read_byte(&(WORD_CHAR_COUNT[idx]));
}
#else
static inline uint8_t get_word_char_count(uint32_t idx)
{
    return WORD_CHAR_COUNT[idx];
}
#endif

// Precomputed match table used by multi_mixed_search DFS. Internal linkage per TU.
static std::vector<std::vector<int>> __pinyin_match_at_global;
// Per-node maximum weight (max weight of any word in this node's subtree). Filled by searches
static std::vector<int> __node_max_weight_global;
static bool __node_max_weight_ready = true;

// helper: find child node for a given token id from a node (or -1)
static inline int node_child_for_token(int node, int tid)
{
    auto er = NODE_EDGE_RANGE[node];
    for (uint32_t e = er[0]; e < er[0] + er[1]; e++) {
        if ((int)NODE_EDGES[e].token_id == tid)
            return NODE_EDGES[e].child_node;
    }
    return -1;
}

// compute per-node max weights using DFS+memo
static inline void compute_node_max_weights()
{
    if (__node_max_weight_ready)
        return;
    size_t node_count = sizeof(NODE_EDGE_RANGE) / sizeof(NODE_EDGE_RANGE[0]);
    __node_max_weight_global.clear();
    __node_max_weight_global.resize(node_count, INT_MIN);
    std::function<int(int)> dfs = [&](int node) -> int {
        if (__node_max_weight_global[node] != INT_MIN)
            return __node_max_weight_global[node];
        int best = INT_MIN;
        auto wr = NODE_WORD_RANGE[node];
        for (uint32_t j = 0; j < wr[1]; j++) {
            uint32_t idx = wr[0] + j;
            int w = get_word_weight(idx);
            if (w > best)
                best = w;
        }
        auto er = NODE_EDGE_RANGE[node];
        for (uint32_t e = er[0]; e < er[0] + er[1]; e++) {
            int child = NODE_EDGES[e].child_node;
            int cw = dfs(child);
            if (cw > best)
                best = cw;
        }
        __node_max_weight_global[node] = (best == INT_MIN) ? INT_MIN : best;
        return __node_max_weight_global[node];
    };
    dfs(0);
    __node_max_weight_ready = true;
}

// Lookup example: walk tokens (each token is a string from PINYIN_TOKENS), return node id or -1
static inline int trie_walk(const char **tokens, size_t ntokens)
{
    int node = 0;
    for (size_t i = 0; i < ntokens; i++) {
        const char *tok = tokens[i];
        auto range = NODE_EDGE_RANGE[node];
        int found = -1;
        for (uint32_t e = range[0]; e < range[0] + range[1]; e++) {
            if (strcmp(PINYIN_TOKENS[NODE_EDGES[e].token_id], tok) == 0) {
                node = NODE_EDGES[e].child_node;
                found = 1;
                break;
            }
        }
        if (found < 0)
            return -1;
    }
    return node;
}

// Abbreviation walk: given initials like "zl", collect ALL matching node ids into out_nodes (max_out).
// Returns the number of matched nodes (0 if none).
static inline int trie_walk_by_initials_all(const char *initials, int *out_nodes, int max_out)
{
    // Use heap-backed vectors to avoid large stack allocations on embedded targets
    std::vector<int> frontier;
    std::vector<int> next_frontier;
    frontier.reserve(256);
    next_frontier.reserve(256);
    frontier.push_back(0);
    for (size_t i = 0; initials[i]; i++) {
        char c = initials[i];
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
        // find initial index
        int init_idx = -1;
        for (int ii = 0; ii < (int)(sizeof(PINYIN_INITIALS) / sizeof(PINYIN_INITIALS[0])); ii++)
            if (PINYIN_INITIALS[ii] == c) {
                init_idx = ii;
                break;
            }
        if (init_idx < 0)
            return 0;
        uint32_t ir_start = INITIAL_TOKEN_RANGE[init_idx][0];
        uint32_t ir_cnt = INITIAL_TOKEN_RANGE[init_idx][1];
        next_frontier.clear();
        for (size_t fi = 0; fi < frontier.size(); fi++) {
            int node = frontier[fi];
            auto range = NODE_EDGE_RANGE[node];
            for (uint32_t e = range[0]; e < range[0] + range[1]; e++) {
                uint16_t tid = NODE_EDGES[e].token_id;
                // check if tid is in INITIAL_TOKEN_IDS[ir_start..ir_start+ir_cnt-1]
                bool match = false;
                for (uint32_t ii = 0; ii < ir_cnt; ii++)
                    if (INITIAL_TOKEN_IDS[ir_start + ii] == tid) {
                        match = true;
                        break;
                    }
                if (match) {
                    int child = NODE_EDGES[e].child_node;
                    bool found = false;
                    for (size_t k = 0; k < next_frontier.size(); k++)
                        if (next_frontier[k] == child) {
                            found = true;
                            break;
                        }
                    if (!found)
                        next_frontier.push_back(child);
                }
            }
        }
        frontier.swap(next_frontier);
        if (frontier.empty())
            return 0;
    }
    // write results to out_nodes (unique already ensured per layer)
    int wrote = 0;
    for (size_t k = 0; k < frontier.size() && wrote < max_out; k++)
        out_nodes[wrote++] = frontier[k];
    return wrote;
}

// Collect candidate WORDS indices for a given initials string (de-duplicated).
// out_word_idx must be able to hold max_out entries; returns actual count.
static inline int collect_candidates_by_initials(const char *initials, uint32_t *out_word_idx, int max_out)
{
    std::vector<int> nodes(1024);
    int n = trie_walk_by_initials_all(initials, nodes.data(), (int)nodes.size());
    if (n == 0)
        return 0;
    // collect word indices from each node and de-duplicate
    int wrote = 0;
    for (int i = 0; i < n; i++) {
        int node = nodes[i];
        auto wr = NODE_WORD_RANGE[node];
        uint32_t start = wr[0];
        uint32_t cnt = wr[1];
        for (uint32_t j = 0; j < cnt; j++) {
            uint32_t idx = start + j;
            bool found = false;
            for (int k = 0; k < wrote; k++)
                if (out_word_idx[k] == idx) {
                    found = true;
                    break;
                }
            if (!found && wrote < max_out)
                out_word_idx[wrote++] = idx;
        }
    }
    // Optionally, sort by weight desc (simple bubble for small lists)
    for (int a = 0; a < wrote; a++)
        for (int b = a + 1; b < wrote; b++) {
            if (get_word_weight(out_word_idx[b]) > get_word_weight(out_word_idx[a])) {
                uint32_t t = out_word_idx[a];
                out_word_idx[a] = out_word_idx[b];
                out_word_idx[b] = t;
            }
        }
    return wrote;
}

// UTF-8 codepoint count helper
static inline int utf8_len(const char *s)
{
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if ((*p & 0xC0) != 0x80)
            n++;
        p++;
    }
    return n;
}

// 1) single-initial fuzzy (e.g. 'c') -> all single-character words whose pinyin initial == c
static inline int single_initial_char_search(char initial, uint32_t *out_word_idx, int max_out)
{
    if (initial >= 'A' && initial <= 'Z')
        initial = initial - 'A' + 'a';
    int init_idx = -1;
    for (int ii = 0; ii < (int)(sizeof(PINYIN_INITIALS) / sizeof(PINYIN_INITIALS[0])); ii++)
        if (PINYIN_INITIALS[ii] == initial) {
            init_idx = ii;
            break;
        }
    if (init_idx < 0)
        return 0;
    uint32_t ir_start = INITIAL_TOKEN_RANGE[init_idx][0];
    uint32_t ir_cnt = INITIAL_TOKEN_RANGE[init_idx][1];
    int wrote = 0;
    auto root_er = NODE_EDGE_RANGE[0];
    for (uint32_t ii = 0; ii < ir_cnt; ii++) {
        uint16_t tid = INITIAL_TOKEN_IDS[ir_start + ii];
        for (uint32_t e = root_er[0]; e < root_er[0] + root_er[1]; e++) {
            if (NODE_EDGES[e].token_id == tid) {
                int child = NODE_EDGES[e].child_node;
                auto wr = NODE_WORD_RANGE[child];
                for (uint32_t j = 0; j < wr[1]; j++) {
                    uint32_t idx = wr[0] + j;
                    {
                        if (get_word_char_count(idx) == 1) {
                            bool found = false;
                            for (int k = 0; k < wrote; k++)
                                if (out_word_idx[k] == idx) {
                                    found = true;
                                    break;
                                }
                            if (!found && wrote < max_out)
                                out_word_idx[wrote++] = idx;
                        }
                    }
                }
            }
        }
    }
    // sort by weight
    for (int a = 0; a < wrote; a++)
        for (int b = a + 1; b < wrote; b++)
            if (get_word_weight(out_word_idx[b]) > get_word_weight(out_word_idx[a])) {
                uint32_t t = out_word_idx[a];
                out_word_idx[a] = out_word_idx[b];
                out_word_idx[b] = t;
            }
    return wrote;
}

// 2) single-full search: full pinyin for a single character (e.g. 'cao')
static inline int single_full_search(const char *full, uint32_t *out_word_idx, int max_out)
{
    int tid = -1;
    for (int i = 0; i < (int)(sizeof(PINYIN_TOKENS) / sizeof(PINYIN_TOKENS[0])); i++)
        if (strcmp(PINYIN_TOKENS[i], full) == 0) {
            tid = i;
            break;
        }
    if (tid < 0)
        return 0;
    int wrote = 0;
    auto er = NODE_EDGE_RANGE[0];
    for (uint32_t e = er[0]; e < er[0] + er[1]; e++) {
        if (NODE_EDGES[e].token_id == tid) {
            int child = NODE_EDGES[e].child_node;
            auto wr = NODE_WORD_RANGE[child];
            for (uint32_t j = 0; j < wr[1]; j++) {
                uint32_t idx = wr[0] + j;
                {
                    if (get_word_char_count(idx) == 1) {
                        bool found = false;
                        for (int k = 0; k < wrote; k++)
                            if (out_word_idx[k] == idx) {
                                found = true;
                                break;
                            }
                        if (!found && wrote < max_out)
                            out_word_idx[wrote++] = idx;
                    }
                }
            }
        }
    }
    for (int a = 0; a < wrote; a++)
        for (int b = a + 1; b < wrote; b++)
            if (get_word_weight(out_word_idx[b]) > get_word_weight(out_word_idx[a])) {
                uint32_t t = out_word_idx[a];
                out_word_idx[a] = out_word_idx[b];
                out_word_idx[b] = t;
            }
    return wrote;
}

// 3) multi-initials fuzzy search (e.g. 'nh' -> n h), returns words whose char count == number of initials
static inline int multi_initials_search(const char *initials, uint32_t *out_word_idx, int max_out)
{
    std::vector<int> nodes(1024);
    int n = trie_walk_by_initials_all(initials, nodes.data(), (int)nodes.size());
    if (n == 0)
        return 0;
    int need_chars = 0;
    for (int i = 0; initials[i]; i++)
        need_chars++;
    int wrote = 0;
    for (int i = 0; i < n; i++) {
        int node = nodes[i];
        auto wr = NODE_WORD_RANGE[node];
        for (uint32_t j = 0; j < wr[1]; j++) {
            uint32_t idx = wr[0] + j;
            if (get_word_char_count(idx) == (uint8_t)need_chars) {
                bool found = false;
                for (int k = 0; k < wrote; k++)
                    if (out_word_idx[k] == idx) {
                        found = true;
                        break;
                    }
                if (!found && wrote < max_out)
                    out_word_idx[wrote++] = idx;
            }
        }
    }
    for (int a = 0; a < wrote; a++)
        for (int b = a + 1; b < wrote; b++)
            if (get_word_weight(out_word_idx[b]) > get_word_weight(out_word_idx[a])) {
                uint32_t t = out_word_idx[a];
                out_word_idx[a] = out_word_idx[b];
                out_word_idx[b] = t;
            }
    return wrote;
}

// 4) multi-full search: input full pinyin without spaces like 'nihao' -> try segment into up to 4 tokens and match
static inline void multi_full_dfs(const char *full, int len, int pos, int depth, int node, int *stack_tid, uint32_t *out_word_idx,
                                  int *wrote, int max_out)
{
    if (pos == len) {
        if (depth == 0)
            return;
        // 'node' is the trie node for the current token sequence
        if (node >= 0) {
            auto wr = NODE_WORD_RANGE[node];
            for (uint32_t j = 0; j < wr[1]; j++) {
                uint32_t idx = wr[0] + j;
                if (get_word_char_count(idx) == (uint8_t)depth) {
                    bool found = false;
                    for (int k = 0; k < *wrote; k++)
                        if (out_word_idx[k] == idx) {
                            found = true;
                            break;
                        }
                    if (!found && *wrote < max_out)
                        out_word_idx[(*wrote)++] = idx;
                }
            }
        }
        return;
    }
    if (depth >= 4)
        return;
    for (int tid = 0; tid < (int)(sizeof(PINYIN_TOKENS) / sizeof(PINYIN_TOKENS[0])); tid++) {
        const char *tk = PINYIN_TOKENS[tid];
        size_t tlen = strlen(tk);
        if ((int)tlen <= len - pos && strncmp(full + pos, tk, tlen) == 0) {
            int child = node_child_for_token(node, tid);
            if (child < 0)
                continue;
            // compute current minimal accepted weight for pruning (if filled)
            int minAccepted = INT_MIN;
            if (*wrote >= max_out) {
                minAccepted = INT_MAX;
                for (int i = 0; i < *wrote; i++) {
                    int w = get_word_weight(out_word_idx[i]);
                    if (w < minAccepted)
                        minAccepted = w;
                }
            }
            if (minAccepted != INT_MIN && (size_t)child < WORDS_MAX_WEIGHT_SIZE && WORDS_MAX_WEIGHT[child] <= minAccepted)
                continue;
            stack_tid[depth] = tid;
            multi_full_dfs(full, len, pos + (int)tlen, depth + 1, child, stack_tid, out_word_idx, wrote, max_out);
        }
    }
}

static inline int multi_full_search(const char *full, uint32_t *out_word_idx, int max_out)
{
    int len = (int)strlen(full);
    int wrote = 0;
    int stack_tid[4];
    // precompute node subtree max weights for pruning
    // compute_node_max_weights();
    multi_full_dfs(full, len, 0, 0, 0, stack_tid, out_word_idx, &wrote, max_out);
    for (int a = 0; a < wrote; a++)
        for (int b = a + 1; b < wrote; b++)
            if (get_word_weight(out_word_idx[b]) > get_word_weight(out_word_idx[a])) {
                uint32_t t = out_word_idx[a];
                out_word_idx[a] = out_word_idx[b];
                out_word_idx[b] = t;
            }
    return wrote;
}

// 4b) multi-mixed search: allow each token to be specified either by its full pinyin or by its initial letter.
// Examples:
//  - "nhao" -> first token initial 'n', second token full "hao"
//  - "nih"  -> first token full "ni", second token initial 'h'
// We traverse the trie edges and at each edge accept the token if the remaining input either starts with the token (full match)
// or has a single letter equal to token[0] (initial match). Depth limited to 4.
static inline void multi_mixed_dfs_walk(const char *full, int len, int pos, int node, int depth,
                                        const std::vector<int> &token_len, uint32_t *out_word_idx, int *wrote, int max_out)
{
    if (pos == len) {
        if (depth == 0)
            return;
        auto wr = NODE_WORD_RANGE[node];
        for (uint32_t j = 0; j < wr[1]; j++) {
            uint32_t idx = wr[0] + j;
            // check character count equals depth
            if (get_word_char_count(idx) != (uint8_t)depth)
                continue;
            bool found = false;
            for (int k = 0; k < *wrote; k++)
                if (out_word_idx[k] == idx) {
                    found = true;
                    break;
                }
            if (!found && *wrote < max_out)
                out_word_idx[(*wrote)++] = idx;
        }
        return;
    }
    if (depth >= 4)
        return;
    auto er = NODE_EDGE_RANGE[node];
    // compute current worst accepted weight for pruning
    int current_worst = INT_MIN;
    if (*wrote >= max_out) {
        current_worst = INT_MAX;
        for (int i = 0; i < *wrote; i++) {
            int w = get_word_weight(out_word_idx[i]);
            if (w < current_worst)
                current_worst = w;
        }
    }
    // next_has_full helps decide initial-first preference
    bool next_has_full =
        (pos + 1 < len) && ((size_t)(pos + 1) < __pinyin_match_at_global.size()) && !__pinyin_match_at_global[pos + 1].empty();
    for (uint32_t e = er[0]; e < er[0] + er[1]; e++) {
        uint16_t tid = NODE_EDGES[e].token_id;
        const char *tk = PINYIN_TOKENS[tid];
        int tlen = token_len[tid];
        int child = NODE_EDGES[e].child_node;

        // quick reject: if neither an initial nor a full match at this pos, skip
        char c = full[pos];
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
        bool init_match = (tk[0] == c);
        bool full_match = false;
        if ((size_t)pos < __pinyin_match_at_global.size()) {
            for (int mid : __pinyin_match_at_global[pos]) {
                if (mid == (int)tid) {
                    full_match = true;
                    break;
                }
            }
        }
        if (!init_match && !full_match)
            continue;

        // subtree pruning: if this child's subtree max weight can't beat current worst, skip
        if (current_worst != INT_MIN && (size_t)child < WORDS_MAX_WEIGHT_SIZE && WORDS_MAX_WEIGHT[child] <= current_worst)
            continue;

        // prefer trying initial-first when helpful (e.g., 'nhao' => 'n' + 'hao')
        if (init_match && next_has_full) {
            // initial first
            multi_mixed_dfs_walk(full, len, pos + 1, child, depth + 1, token_len, out_word_idx, wrote, max_out);
            if (full_match)
                multi_mixed_dfs_walk(full, len, pos + tlen, child, depth + 1, token_len, out_word_idx, wrote, max_out);
        } else {
            if (full_match)
                multi_mixed_dfs_walk(full, len, pos + tlen, child, depth + 1, token_len, out_word_idx, wrote, max_out);
            if (init_match)
                multi_mixed_dfs_walk(full, len, pos + 1, child, depth + 1, token_len, out_word_idx, wrote, max_out);
        }
    }
}

static inline int multi_mixed_search(const char *full, uint32_t *out_word_idx, int max_out)
{
    int len = (int)strlen(full);
    int wrote = 0;
    // precompute subtree max weights for pruning
    compute_node_max_weights();
    // Precompute token lengths and match positions to avoid scanning tokens repeatedly in DFS
    int token_count = (int)(sizeof(PINYIN_TOKENS) / sizeof(PINYIN_TOKENS[0]));
    std::vector<int> token_len(token_count);
    for (int i = 0; i < token_count; i++)
        token_len[i] = (int)strlen(PINYIN_TOKENS[i]);
    __pinyin_match_at_global.clear();
    __pinyin_match_at_global.resize((len > 0) ? len : 1);
    for (int pos = 0; pos < len; pos++) {
        for (int tid = 0; tid < token_count; tid++) {
            int tlen = token_len[tid];
            if (tlen <= len - pos && strncmp(full + pos, PINYIN_TOKENS[tid], tlen) == 0) {
                __pinyin_match_at_global[pos].push_back(tid);
            }
        }
    }
    multi_mixed_dfs_walk(full, len, 0, 0, 0, token_len, out_word_idx, &wrote, max_out);
    // sort by weight desc
    for (int a = 0; a < wrote; a++)
        for (int b = a + 1; b < wrote; b++)
            if (get_word_weight(out_word_idx[b]) > get_word_weight(out_word_idx[a])) {
                uint32_t t = out_word_idx[a];
                out_word_idx[a] = out_word_idx[b];
                out_word_idx[b] = t;
            }
    return wrote;
}

// Unified search: accept arbitrary input string and return up to max_out WORDS indices.
// Strategy (same as demo):
// 1) try multi_full_search (treat input as concatenated full pinyin)
// 2) if input length == 1: try single_full_search, then single_initial_char_search
// 3) try multi_initials_search
// 4) fallback single_full_search
// The function deduplicates results and returns them sorted by weight (desc).
static inline int unified_search(const char *input, uint32_t *out_word_idx, int max_out)
{
    std::vector<uint32_t> tmp;
    tmp.resize(1024);
    int got = 0;
    int total = 0;
    /*
    // try multi-full
    got = multi_full_search(input, tmp.data(), (int)tmp.size());
    if (got>0) {
        // copy top up to max_out (dedupe not needed since multi_full_search de-dupes)
        int n = (got < max_out) ? got : max_out;
        for (int i=0;i<n;i++) out_word_idx[total++]=tmp[i];
        return total;
    }
    size_t len = strlen(input);
    if (len==1) {
        // single full
    got = single_full_search(input, tmp.data(), (int)tmp.size());
        for (int i=0;i<got && total<max_out;i++) out_word_idx[total++]=tmp[i];
        if (total>=max_out) return total;
        // single-initial
    got = single_initial_char_search(input[0], tmp.data(), (int)tmp.size());
        for (int i=0;i<got && total<max_out;i++) {
            // dedupe
            uint32_t v = tmp[i]; bool found=false; for (int k=0;k<total;k++) if (out_word_idx[k]==v) { found=true; break; }
            if (!found) out_word_idx[total++]=v;
        }
        return total;
    }

    // try multi-initials
    got = multi_initials_search(input, tmp.data(), (int)tmp.size());
    for (int i=0;i<got && total<max_out;i++) { uint32_t v=tmp[i]; bool found=false; for (int k=0;k<total;k++) if
    (out_word_idx[k]==v) { found=true; break; } if (!found) out_word_idx[total++]=v; } if (total>0) return total;
    // fallback single_full
    got = single_full_search(input, tmp.data(), (int)tmp.size());
    for (int i=0;i<got && total<max_out;i++) { uint32_t v=tmp[i]; bool found=false; for (int k=0;k<total;k++) if
    (out_word_idx[k]==v) { found=true; break; } if (!found) out_word_idx[total++]=v; }
    // final sort by weight (desc)
    for (int a=0;a<total;a++) for (int b=a+1;b<total;b++) if (get_word_weight(out_word_idx[b]) > get_word_weight(out_word_idx[a]))
    { uint32_t t=out_word_idx[a]; out_word_idx[a]=out_word_idx[b]; out_word_idx[b]=t; } if (total>0) return total;
    */

    // Mixed multi-token fuzzy: allow per-token initial or full matches (e.g. "nhao" or "nih").
    // Try mixed-mode search where each token may be matched by its initial (single letter) or full pinyin.
    got = multi_mixed_search(input, tmp.data(), (int)tmp.size());
    for (int i = 0; i < got && total < max_out; i++) {
        uint32_t v = tmp[i];
        bool found = false;
        for (int k = 0; k < total; k++)
            if (out_word_idx[k] == v) {
                found = true;
                break;
            }
        if (!found)
            out_word_idx[total++] = v;
    }
    return total;
}

// C++-friendly overload: accept std::string and return vector<string> of top matches (words only)
static inline std::vector<std::string> unified_search(const std::string &input, int max_out = 20)
{
    if (input == lastPinyin)
        return lastresult;
    lastresult.clear();
    std::vector<uint32_t> idxs;
    idxs.resize(max_out > 0 ? max_out : 1);
    int got = unified_search(input.c_str(), idxs.data(), max_out);
    for (int i = 0; i < got; i++) {
        std::vector<char> tmp_buf(64);
        get_word_ptr_buf(idxs[i], tmp_buf.data(), tmp_buf.size());
        lastresult.emplace_back(std::string(tmp_buf.data()));
    }
    lastPinyin = input;
    return lastresult;
}

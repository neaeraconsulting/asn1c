#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <xer_support.h>
#include <jer_support.h>

/*
 * The XML and JSON tokenizers run over the characters which change nothing
 * in a tight loop. The expectations below were recorded from the tokenizers
 * as they were before that, which went through the state machine for every
 * character: the tokens of the whole text, and for every prefix of the text
 * the number of bytes consumed, the state reached and the number of tokens.
 */

static const char xml_text[] =
    "t <a>12</a><b x=\"1>2\" y=z ><!-- c - -- --><c/> 3 < 4 <d<e>&amp;</e ></a>x";
static const int xml_tokens[][3] = { /* type, offset, size */
    {0, 0, 2},
    {3, 2, 3},
    {0, 5, 2},
    {3, 7, 4},
    {3, 11, 16},
    {4, 27, 15},
    {3, 42, 4},
    {0, 46, 3},
    {0, 49, 2},
    {0, 51, 2},
    {3, 53, 2},
    {3, 55, 3},
    {0, 58, 5},
    {3, 63, 5},
    {3, 68, 4},
    {0, 72, 1},
};
static const int xml_prefixes[][3] = { /* consumed, state, tokens */
    {0, 0, 0}, {1, 0, 1}, {2, 0, 1}, {2, 1, 1}, {2, 2, 1}, {5, 0, 2},
    {6, 0, 3}, {7, 0, 3}, {7, 1, 3}, {7, 2, 3}, {7, 2, 3}, {11, 0, 4},
    {11, 1, 4}, {11, 2, 4}, {11, 2, 4}, {11, 2, 4}, {11, 3, 4}, {11, 4, 4},
    {11, 4, 4}, {11, 4, 4}, {11, 4, 4}, {11, 2, 4}, {11, 2, 4}, {11, 2, 4},
    {11, 3, 4}, {11, 5, 4}, {11, 2, 4}, {27, 0, 5}, {27, 1, 5}, {27, 6, 5},
    {27, 7, 5}, {31, 8, 6}, {32, 8, 6}, {33, 8, 6}, {34, 8, 6}, {27, 9, 5},
    {36, 8, 6}, {27, 9, 5}, {27, 10, 5}, {39, 8, 6}, {27, 9, 5}, {27, 10, 5},
    {42, 0, 6}, {42, 1, 6}, {42, 2, 6}, {42, 2, 6}, {46, 0, 7}, {47, 0, 8},
    {48, 0, 8}, {49, 0, 8}, {49, 1, 8}, {51, 0, 9}, {52, 0, 10}, {53, 0, 10},
    {53, 1, 10}, {53, 2, 10}, {55, 1, 11}, {55, 2, 11}, {58, 0, 12}, {59, 0, 13},
    {60, 0, 13}, {61, 0, 13}, {62, 0, 13}, {63, 0, 13}, {63, 1, 13}, {63, 2, 13},
    {63, 2, 13}, {63, 2, 13}, {68, 0, 14}, {68, 1, 14}, {68, 2, 14}, {68, 2, 14},
    {72, 0, 15}, {73, 0, 16},
};

static const char json_text[] =
    "{\"a\":1,\"b c\": \"x,\\\"}y\" ,\"d\":[1, {\"e\":[]},\"f\\\\\",[2,3]],\"g\":{}, \"h\" : null}";
static const int json_tokens[][3] = { /* type, offset, size */
    {3, 0, 1},
    {4, 1, 3},
    {0, 4, 1},
    {5, 5, 1},
    {0, 6, 1},
    {4, 7, 5},
    {0, 12, 2},
    {5, 14, 2},
    {0, 16, 2},
    {4, 18, 4},
    {0, 22, 6},
    {5, 28, 2},
    {0, 30, 3},
    {4, 33, 3},
    {0, 36, 1},
    {5, 37, 2},
    {5, 39, 21},
    {5, 60, 12},
    {5, 72, 1},
};
static const int json_prefixes[][3] = { /* consumed, state, tokens */
    {0, 0, 0}, {1, 1, 1}, {1, 2, 1}, {1, 2, 1}, {4, 3, 2}, {4, 4, 2},
    {5, 5, 3}, {6, 1, 4}, {7, 2, 5}, {7, 2, 5}, {7, 2, 5}, {7, 2, 5},
    {12, 3, 6}, {12, 4, 6}, {12, 4, 6}, {14, 5, 7}, {14, 5, 7}, {16, 1, 8},
    {16, 1, 8}, {18, 2, 9}, {18, 2, 9}, {18, 2, 9}, {22, 3, 10}, {22, 3, 10},
    {22, 3, 10}, {22, 3, 10}, {22, 3, 10}, {22, 3, 10}, {22, 4, 10}, {28, 5, 11},
    {28, 5, 11}, {30, 1, 12}, {30, 1, 12}, {30, 1, 12}, {33, 2, 13}, {33, 2, 13},
    {36, 3, 14}, {36, 4, 14}, {37, 5, 15}, {37, 5, 15}, {39, 8, 16}, {39, 8, 16},
    {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16},
    {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16},
    {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16}, {39, 8, 16},
    {60, 0, 17}, {61, 0, 18}, {62, 0, 18}, {63, 0, 18}, {64, 0, 18}, {65, 0, 18},
    {66, 0, 18}, {67, 0, 18}, {68, 0, 18}, {69, 0, 18}, {70, 0, 18}, {71, 0, 18},
    {72, 0, 18}, {73, 0, 19},
};

static const char json_text_text[] =
    "\"q\\\"r,s]\\\\\" , x\\\"y, [\"t\"] \"u}\\\\\\\"v\",w";
static const int json_text_tokens[][3] = { /* type, offset, size */
    {5, 0, 12},
    {5, 12, 6},
    {3, 18, 3},
    {5, 21, 3},
    {5, 24, 11},
    {5, 35, 2},
};
static const int json_text_prefixes[][3] = { /* consumed, state, tokens */
    {0, 0, 0}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {4, 0, 1}, {5, 0, 1},
    {6, 0, 1}, {7, 0, 1}, {8, 0, 1}, {9, 0, 1}, {10, 0, 1}, {11, 0, 1},
    {12, 0, 1}, {13, 0, 2}, {14, 0, 2}, {15, 0, 2}, {16, 0, 2}, {17, 0, 2},
    {18, 0, 2}, {19, 0, 3}, {20, 0, 3}, {21, 6, 3}, {21, 7, 3}, {21, 7, 3},
    {21, 7, 3}, {25, 0, 5}, {26, 0, 5}, {27, 0, 5}, {28, 0, 5}, {29, 0, 5},
    {30, 0, 5}, {31, 0, 5}, {32, 0, 5}, {33, 0, 5}, {34, 0, 5}, {35, 0, 5},
    {36, 0, 6}, {37, 0, 6},
};

struct tokens {
    const char *base;
    int count;
    int stop_after;    /* Refuse the token after this many, if not negative */
    int seen[64][3];
};

static int
record(struct tokens *t, int type, const void *chunk, size_t size) {
    if(t->stop_after >= 0 && t->count == t->stop_after) return -1;
    assert(t->count < 64);
    t->seen[t->count][0] = type;
    t->seen[t->count][1] = (const char *)chunk - t->base;
    t->seen[t->count][2] = size;
    t->count++;
    return size;
}

static int
xml_cb(pxml_chunk_type_e type, const void *chunk, size_t size, void *key) {
    return record((struct tokens *)key, type, chunk, size);
}

static int
json_cb(pjson_chunk_type_e type, const void *chunk, size_t size, void *key) {
    return record((struct tokens *)key, type, chunk, size);
}

static ssize_t
parse(int json, int *state, const char *text, size_t size, struct tokens *t) {
    return json ? pjson_parse(state, text, size, json_cb, t)
                : pxml_parse(state, text, size, xml_cb, t);
}

static void
check(int json, const char *text, const int (*tokens)[3], int tokens_count,
      const int (*prefixes)[3]) {
    size_t len = strlen(text);
    struct tokens t;
    size_t n;
    int state;
    int i;

    /* The whole text at once. */
    memset(&t, 0, sizeof(t));
    t.base = text;
    t.stop_after = -1;
    state = 0;
    parse(json, &state, text, len, &t);
    assert(t.count == tokens_count);
    assert(memcmp(t.seen, tokens, tokens_count * sizeof(tokens[0])) == 0);

    /* The text cut short at every position. */
    for(n = 0; n <= len; n++) {
        ssize_t consumed;
        t.count = 0;
        state = 0;
        consumed = parse(json, &state, text, n, &t);
        assert(consumed == prefixes[n][0]);
        assert(state == prefixes[n][1]);
        assert(t.count == prefixes[n][2]);
    }

    /*
     * The way the decoders do it: stop after each token,
     * and start again where it ended.
     */
    for(i = 0; i <= tokens_count; i++) {
        ssize_t consumed;
        t.count = 0;
        t.stop_after = i;
        state = 0;
        consumed = parse(json, &state, text, len, &t);
        assert(t.count == i);
        assert(memcmp(t.seen, tokens, i * sizeof(tokens[0])) == 0);
        if(i < tokens_count) {
            assert(consumed == tokens[i][1]);
        } else {
            assert(consumed == (ssize_t)len);
        }
    }

    printf("%s: %d tokens, %d prefixes\n", json ? "JSON" : "XML", tokens_count,
           (int)len + 1);
}

int
main() {
    check(0, xml_text, xml_tokens, sizeof(xml_tokens) / sizeof(xml_tokens[0]),
          xml_prefixes);
    check(1, json_text, json_tokens,
          sizeof(json_tokens) / sizeof(json_tokens[0]), json_prefixes);
    /* Strings and escapes outside of an object or an array. */
    check(1, json_text_text, json_text_tokens,
          sizeof(json_text_tokens) / sizeof(json_text_tokens[0]),
          json_text_prefixes);
    return 0;
}

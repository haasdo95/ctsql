def gen_keyword(kw: str):
    pattern = "".join(map(lambda c: '[' + str(c).lower() + str(c).upper() + ']', kw))
    return f"""
static constexpr char {kw}_pattern[] = \"{pattern}\";
static constexpr ctpg::regex_term<{kw}_pattern> {kw}_kw(\"{kw}_kw\");"""

if __name__ == '__main__':
    keywords = ["select", "as", "from", "where", "count", "sum", "max", "min", "avg", "not", "and",  "or"]
    print(", ".join(map(lambda k: k+"_kw", keywords)))
    for k in keywords:
        print(gen_keyword(k))

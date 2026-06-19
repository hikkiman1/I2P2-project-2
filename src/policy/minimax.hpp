#pragma once
#include "search_types.hpp"
#include "game_history.hpp"

struct MMParams {
    bool use_kp_eval = true;
    bool use_eval_mobility = true;
    bool report_partial = true;

    static MMParams from_map(const ParamMap& m){
        MMParams p;
        p.use_kp_eval       = param_bool(m, "UseKPEval", true);
        p.use_eval_mobility = param_bool(m, "UseEvalMobility", true);
        p.report_partial    = param_bool(m, "ReportPartial", true);
        return p;
    }
};
// add tranposition table

enum TTFlag{
    TT_exact,  //AI have reach the end of the branch and return the exact point
    TT_lowerbound,    // beta cutoff, oppn will not let a good point > beta happen, keep the minimum pts (min = beta)
    TT_uppderbound    //  all the move is worse than alpha, save the maximum (worst = alpha)
};

struct TTEntry{
    uint64_t key = 0;   // Zobrist Hash code
    int depth = -1;     // Depth that has been checked
    int score = 0;      // Best score
    Move best_move;     // Best move
    TTFlag flag;        // flag to know the pts
};

class MiniMax{
public:
    static int eval_ctx(
        State *state,
        int depth,
        // i add this for alpha_pruning
        int alpha,
        int beta,
        GameHistory& history,
        int ply,
        SearchContext& ctx,
        const MMParams& p
    );
    static int quiescence_search(
        State *state,
        int alpha,
        int beta,
        GameHistory& history,
        int ply,
        SearchContext& ctx,
        const MMParams& p
    );
    static SearchResult search(
        State *state,
        int depth,
        GameHistory& history,
        SearchContext& ctx
    );

    static ParamMap default_params();
    static std::vector<ParamDef> param_defs();
};

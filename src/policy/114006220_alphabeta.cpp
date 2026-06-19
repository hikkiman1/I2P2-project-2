#include <utility>
#include "state.hpp"
#include "114006220_alphabeta.hpp"


/*============================================================
 * MiniMax — eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
 *============================================================*/
int AlphaBeta::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */

    // [ Hackathon TODO 3-1 ]
    // return the score for a winning terminal state
    // Hint: prefer faster wins by using ply.
    if (state->game_state == WIN){
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    history.push(state->hash());

    if(depth <= 0){
        int score = state->evaluate(
            p.use_kp_eval, p.use_eval_mobility, &history
        ); 
        history.pop(state->hash());
        return score;
    }

    /* === Negamax loop === */
    int best_score = M_MAX;


    for(auto& action : state->legal_actions){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        // [Hackathon TODO 3-3]
        // search the child one level deeper
        int raw_score;
        if (same) {
            // If the player doesn't change, pass alpha and beta normally
            raw_score = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
        } else {
            // Negamax: If it's the opponent's turn, flip and negate the bounds
            raw_score = eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
        }


        // [Hackathon TODO 3-4]
        // convert raw to the current player's perspective.
        int score = same?raw_score:-raw_score;
        delete next;

        // [ Hackathon TODO 3-5 ]
        // update best_score if this child is better.
        if (score > best_score){
            best_score = score;
        }
        if (best_score > alpha){
            alpha = best_score;
        } 
        if (alpha >= beta) {
            break;
        }
    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * MiniMax — search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult AlphaBeta::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }


    int best_score = M_MAX - 10;
    // alpha_beta pruning
    int alpha = M_MAX;
    int beta = P_MAX;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action : state->legal_actions){
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
            State* next = state->next_state(action);

            bool same = next->same_player_as_parent();

            // change from todo 3, ply + 1 to just 1 since we looks at the first move first
            int raw_score;
            if (same) {
                raw_score = eval_ctx(next, depth - 1, alpha, beta, history, 1, ctx, p);
            } else {
                raw_score = eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
            }


            int score = same?raw_score:-raw_score;
            delete next;

            if(score > best_score){
                // [ Hackathon TODO 4-2 ]
                // keep this move if it is the best so far
                best_score =  score;
                result.best_move = action;

                if(p.report_partial && ctx.on_root_update){
                   ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
                }
            }  
            //update alpha at the root level so next moves benefit from pruning
            if (best_score > alpha) {
                alpha = best_score;
            }
        move_index++;
    }

    // [ Hackathon TODO 4-3 ]
    // update result and return
        result.score = best_score;
        result.nodes = ctx.nodes;
        result.pv = {result.best_move};
        return result;
} 


/*============================================================
 * MiniMax — default_params / param_defs
 *============================================================*/
ParamMap AlphaBeta::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}

#include <utility>
#include <algorithm>
#include "state.hpp"
#include "quiescence.hpp"


/*============================================================
 * quiescene — eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
 *============================================================*/
int quiescene::eval_ctx(
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
    
    //now when we reach depth = 0, before we stop, we check the capture moves to avoid horizon effect
    if(depth <= 0){
        int score = quiescence_search(
            state, alpha, beta, history, ply, ctx, p
        ); 
        history.pop(state->hash());
        return score;
    }


    int oppn = 1 -state->player;

    //reordering the actions base on most valuable victim - least valuable attacker
    int self = state->player;

    std::sort(state->legal_actions.begin(), state->legal_actions.end(), [&](const Move& m1, const Move& m2){
        //evaluate move 1
        int atk1 = state->board.board[self][m1.first.first][m1.first.second];
        int vic1 = state->board.board[oppn][m1.second.first][m1.second.second];
        int score1 = 0;
        if (vic1 > 0){
            score1 = PIECE_VALUES[vic1] * 10 - PIECE_VALUES[atk1];
        }
        //evaluate move 2
        int atk2 = state->board.board[self][m2.first.first][m2.first.second];
        int vic2 = state->board.board[oppn][m2.second.first][m2.second.second];
        int score2 = 0;
        if (vic2 > 0){
            score2 = PIECE_VALUES[vic2] * 10 - PIECE_VALUES[atk2];
        }

        return score1 > score2;
    });


    
    /* === Negamax loop === */
    int best_score = M_MAX;
    bool first_move = true; //for pvs, using the first branch of each node as an anchor

    for(auto& action : state->legal_actions){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        // [Hackathon TODO 3-3]
        // search the child one level deeper
        int raw_score;
        // Here we start implement the PVS
        if (first_move){
            // run the same alpha-beta pruning in first move
            if (same) {
                // If the player doesn't change, pass alpha and beta normally
                raw_score = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
            } else {
                // Negamax: If it's the opponent's turn, flip and negate the bounds
                raw_score = eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
            }
        } else {
            // for other branches, we just use a null window of alpha and alpha + 1 to just see if this branch score is bigger than alpha
            if (same) {
                raw_score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, ply + 1, ctx, p);
            } else {
                raw_score = eval_ctx(next, depth - 1, -(alpha + 1), -alpha, history, ply + 1, ctx, p);
            }
            // convert to current perspective to check if we need to research
            int score = same ? raw_score : -raw_score;
            //if the score is good and still lower than beta, research again with score as alpha
            if (score > alpha && score < beta){
                if (same) {
                    raw_score = eval_ctx(next, depth - 1, score, beta, history, ply + 1, ctx, p);
                } else {
                    raw_score = eval_ctx(next, depth - 1, -beta, -score, history, ply + 1, ctx, p);
                }
            }
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


        //so that we start quick-check the other branches
        first_move = false;
    }

    history.pop(state->hash());
    return best_score;
}


int quiescene::quiescence_search(
    State *state, 
    int alpha, 
    int beta, 
    GameHistory &history, 
    int ply, 
    SearchContext &ctx, 
    const MMParams &p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
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

    // we check the point here immediatly, a static evaluation
    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);

    if (stand_pat >= beta){
        return beta;
        //stop since opponent sure gonna not let us choose this move
    } 
    if (stand_pat > alpha){
        //update alpha if standing pat is better than our minimum guaranteed score
        alpha = stand_pat;
    }
    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    
    //search only capture moves
    int oppn = 1 -state->player;

    //reordering the actions base on most valuable victim - least valuable attacker
    int self = state->player;

    std::sort(state->legal_actions.begin(), state->legal_actions.end(), [&](const Move& m1, const Move& m2){
        //evaluate move 1
        int atk1 = state->board.board[self][m1.first.first][m1.first.second];
        int vic1 = state->board.board[oppn][m1.second.first][m1.second.second];
        int score1 = 0;
        if (vic1 > 0){
            score1 = PIECE_VALUES[vic1] * 10 - PIECE_VALUES[atk1];
        }
        //evaluate move 2
        int atk2 = state->board.board[self][m2.first.first][m2.first.second];
        int vic2 = state->board.board[oppn][m2.second.first][m2.second.second];
        int score2 = 0;
        if (vic2 > 0){
            score2 = PIECE_VALUES[vic2] * 10 - PIECE_VALUES[atk2];
        }

        return score1 > score2;
    });

    for (auto& action : state->legal_actions){
        int target_r = action.second.first;
        int target_c = action.second.second;

        //check capture
        if (state->board.board[oppn][target_r][target_c] > 0){

            State* next = state->next_state(action);
            bool same = next->same_player_as_parent();

            int raw_score;
            //use negamax logic for capture move
            if (same){
                raw_score = quiescence_search(next, alpha, beta, history, ply + 1, ctx, p);
            } else {
                raw_score = quiescence_search(next, -beta, -alpha, history, ply + 1, ctx, p);
            }
            int score = same ? raw_score : -raw_score;

            delete next;

            if (score >= beta){
                return beta;
            }
            if (score > alpha){
                alpha = score;
            }
        }
    }
    return alpha;
}




/*============================================================
 * quiescene — search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult quiescene::search(
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
    int oppn = 1 -state->player;

    //reordering the actions base on most valuable victim - least valuable attacker
    int self = state->player;

    std::sort(state->legal_actions.begin(), state->legal_actions.end(), [&](const Move& m1, const Move& m2){
        //evaluate move 1
        int atk1 = state->board.board[self][m1.first.first][m1.first.second];
        int vic1 = state->board.board[oppn][m1.second.first][m1.second.second];
        int score1 = 0;
        if (vic1 > 0){
            score1 = PIECE_VALUES[vic1] * 10 - PIECE_VALUES[atk1];
        }
        //evaluate move 2
        int atk2 = state->board.board[self][m2.first.first][m2.first.second];
        int vic2 = state->board.board[oppn][m2.second.first][m2.second.second];
        int score2 = 0;
        if (vic2 > 0){
            score2 = PIECE_VALUES[vic2] * 10 - PIECE_VALUES[atk2];
        }

        return score1 > score2;
    });

    int best_score = M_MAX - 10;
    // alpha_beta pruning
    int alpha = M_MAX;
    int beta = P_MAX;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    //same as eval_ctx
    bool first_move = true;

    for(auto& action : state->legal_actions){
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
            State* next = state->next_state(action);

            bool same = next->same_player_as_parent();

            // change from todo 3, ply + 1 to just 1 since we looks at the first move first
            int raw_score;
            if (first_move){
            // run the same alpha-beta pruning in first move
            if (same) {
                // If the player doesn't change, pass alpha and beta normally
                raw_score = eval_ctx(next, depth - 1, alpha, beta, history, 1, ctx, p);
            } else {
                // Negamax: If it's the opponent's turn, flip and negate the bounds
                raw_score = eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
            }
        } else {
            // for other branches, we just use a null window of alpha and alpha + 1 to just see if this branch score is bigger than alpha
            if (same) {
                raw_score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, 1, ctx, p);
            } else {
                raw_score = eval_ctx(next, depth - 1, -(alpha + 1), -alpha, history, 1, ctx, p);
            }
            // convert to current perspective to check if we need to research
            int score = same ? raw_score : -raw_score;
            //if the score is good and still lower than beta, research again with score as alpha
            if (score > alpha && score < beta){
                if (same) {
                    raw_score = eval_ctx(next, depth - 1, score, beta, history, 1, ctx, p);
                } else {
                    raw_score = eval_ctx(next, depth - 1, -beta, -score, history, 1, ctx, p);
                }
            }
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

        first_move = false;
    }

    // [ Hackathon TODO 4-3 ]
    // update result and return
        result.score = best_score;
        result.nodes = ctx.nodes;
        result.pv = {result.best_move};
        return result;
} 


/*============================================================
 * quiescene — default_params / param_defs
 *============================================================*/
ParamMap quiescene::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> quiescene::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}

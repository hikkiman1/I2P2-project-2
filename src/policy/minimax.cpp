#include <utility>
#include <algorithm>
#include "state.hpp"
#include "minimax.hpp"

// create a note so that we can look up the move we did using the tranposition table
const int TT_SIZE = 1 << 20;
TTEntry TT[TT_SIZE];

// implement killer move, 2 killer moves, maximum of 100 depths
Move killer_moves[100][2];

/*============================================================
 * MiniMax — eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
 *============================================================*/
int MiniMax::eval_ctx(
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
    
    // Check the transposition table
    int alpha_ori = alpha;  // save the original alpha so that we can check which type of flag it is
    uint64_t hash_key = state->hash();
    int tt_index = hash_key & (TT_SIZE - 1); // using bitwise to find the idx in array
    TTEntry& tte = TT[tt_index];

    Move tt_best_move;
    bool has_tt_move = false;

    //check if it got the same hash code
    if (tte.key == hash_key){
        has_tt_move = true;
        tt_best_move = tte.best_move; // get the best move first
        // only use past move if their depth is same or higher
        if (tte.depth >= depth){
            if (tte.flag == TT_exact){
                history.pop(state->hash());
                return tte.score;
            } else if (tte.flag == TT_lowerbound){
                alpha = std::max(alpha, tte.score);
            } else if (tte.flag == TT_uppderbound){
                beta = std::min(beta, tte.score);
            }
            if (alpha >= beta){
                history.pop(state->hash());
                return tte.score; // alpha-beta pruning logic
            }
        }
    }


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
        //we priotize the move we got from the transposition table
        if (has_tt_move){
            if (m1 == tt_best_move) return true;
            if (m2 == tt_best_move) return false;
        }
        //2nd priotize the killer move
        bool m1_is_killer = (m1 == killer_moves[ply][0] || m1 == killer_moves[ply][1]);
        bool m2_is_killer = (m2 == killer_moves[ply][0] || m2 == killer_moves[ply][1]);
        if (m1_is_killer && !m2_is_killer) return true;
        if (m2_is_killer && !m1_is_killer) return false;


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
    Move best_move_in_node; //to save best move in node to save into our transposition table
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
            best_move_in_node = action; // update best move for TT
        }
        if (best_score > alpha){
            alpha = best_score;
        } 
        if (alpha >= beta) {
            // (killer move) if a move make pruning and it is not a capture moves, we save it
            if (state->board.board[oppn][action.second.first][action.second.second]){
                // push old killer to 2nd pos, new killer to 1st pos
                if (action!=killer_moves[ply][0]){
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = action;
                }
            }
            break;
        }

        //so that we start quick-check the other branches
        first_move = false;
    }

    // put it into our TT
    TTFlag flag;
    if (best_score <= alpha_ori){
        flag = TT_uppderbound;
    } else if (best_score >= beta){
        flag = TT_lowerbound;
    } else {
        flag = TT_exact;
    }

    //overwrite the old info in TT
    tte.key = hash_key;
    tte.depth = depth;
    tte.score = best_score;
    tte.best_move = best_move_in_node;
    tte.flag = flag;

    history.pop(state->hash());
    return best_score;
}


int MiniMax::quiescence_search(
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
 * MiniMax — search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult MiniMax::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    (void) depth;

    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = 1;

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


    //implement iterative deepening
    int total_moves = (int)state->legal_actions.size();
    if (total_moves == 0) return result;
    
    int current_depth = 1;
    while (!ctx.stop){
        int best_score = M_MAX - 10;
        // alpha_beta pruning
        int alpha = M_MAX;
        int beta = P_MAX;
        int move_index = 0;
        bool first_move = true;
        SearchResult current_depth_result; //temporary result of this loop

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
                raw_score = eval_ctx(next, current_depth - 1, alpha, beta, history, 1, ctx, p);
            } else {
                // Negamax: If it's the opponent's turn, flip and negate the bounds
                raw_score = eval_ctx(next, current_depth - 1, -beta, -alpha, history, 1, ctx, p);
            }
            } else {
                // for other branches, we just use a null window of alpha and alpha + 1 to just see if this branch score is bigger than alpha
                if (same) {
                    raw_score = eval_ctx(next, current_depth - 1, alpha, alpha + 1, history, 1, ctx, p);
                } else {
                    raw_score = eval_ctx(next, current_depth - 1, -(alpha + 1), -alpha, history, 1, ctx, p);
                }
                // convert to current perspective to check if we need to research
                int score = same ? raw_score : -raw_score;
                //if the score is good and still lower than beta, research again with score as alpha
                if (score > alpha && score < beta){
                    if (same) {
                        raw_score = eval_ctx(next, current_depth - 1, score, beta, history, 1, ctx, p);
                    } else {
                        raw_score = eval_ctx(next, current_depth - 1, -beta, -score, history, 1, ctx, p);
                    }
                }
            }


            int score = same?raw_score:-raw_score;
            delete next;
            if (ctx.stop) break; //if run out of time

            if(score > best_score){
                // [ Hackathon TODO 4-2 ]
                // keep this move if it is the best so far
                best_score =  score;
                current_depth_result.best_move = action;
                current_depth_result.score = best_score;
                current_depth_result.nodes = ctx.nodes;
                current_depth_result.pv = {action};

                if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({action, best_score, current_depth, move_index + 1, total_moves});
                }
            }  
            //update alpha at the root level so next moves benefit from pruning
            if (best_score > alpha) {
                alpha = best_score;
            }
            move_index++;

            first_move = false;
        }
        if (ctx.stop) break; //if run out of time

        //update final result
        result = current_depth_result; 
        result.depth = current_depth;

        //push the best move to the head of the list for next depth
        if (result.best_move != state->legal_actions[0]){
            auto it = std::find(state->legal_actions.begin(), state->legal_actions.end(), result.best_move);
            if (it != state->legal_actions.end()){
                std::rotate(state->legal_actions.begin(), it, it + 1);
            }
        }

        if (best_score >= P_MAX - 10) break; //already find the checkmate

        current_depth++;
    }
        return result;
} 


/*============================================================
 * MiniMax — default_params / param_defs
 *============================================================*/
ParamMap MiniMax::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> MiniMax::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}

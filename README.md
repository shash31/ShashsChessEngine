# C++ Chess Engine (~2400 elo)

This is a UCI compliant Chess Engine built in C++ with an automated script that plays against chess.com bots.
The automation script (chesscombot.py) which is written with playwright is used to test the playing strength of the chess engine against other bots.
It has beaten a 2900 elo bot up till now. The recorded games can be seen in games.pgn

## Features:
- Magic Bitboards(O(1) move generation)
- Negamax alpha beta search
- Transposition table (w/ Zobrist hashing)
- Iterative Deepening Search
- Move ordering heuristics(TT moves, MVV-LVA Captures, Killer moves, history moves)
- Principal Variation Search
- Null move pruning
- Late Move Reduction
- Aspiration Windows
- Reverse Futility Pruning
- 

## To-Dos
- Conduct SPRT tests to fine-tune certain values(Null move pruning depth reduction factor, LMR factor, aspiration window margin, etc.)
- Improve mate searching (has search instability and slight mate blindness at times because of the fancy search improvements like null move pruning, LMR, etc.)
- Connect to NNUE for better evaluation

You can also play against this bot on this [site](https://shash31.github.io/ChessEngine) which is still a work in progress.

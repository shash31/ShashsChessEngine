import subprocess
from playwright.sync_api import sync_playwright

PLAYING_BLACK = False
TIME_PER_MOVE = 10000 # in ms

OPP_DIFFICULTY = "25"
# Has beaten all bots performing at 3200 ELO wtf
# 1-6: 150 increment from 250-1000
# 6-23: 100 increment from 1000-2700
# 24: 2900
# 25: 3200

engine = subprocess.Popen(
    ['./rfp_engine'],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
    bufsize=1 # Line-buffered
)

def get_engine_move(fen):
    engine.stdin.write(f"position fen {fen}\n")
    engine.stdin.write(f"go movetime {TIME_PER_MOVE}\n")
    engine.stdin.write("\n")
    engine.stdin.flush()
    
    while True:
        line = engine.stdout.readline().strip()
        print(line)
        if line.startswith("bestmove"):
            return line.split()[1] # Returns e.g., "e2e4"


def wait_for_opp_move(page):
    """
    Pause execution until the opponent completes a move by tracking 
    the board's move highlighting elements.
    """

    last_move_highlight = page.locator("wc-chess-board .highlight")
    old_highlights = [el.get_attribute("class") for el in last_move_highlight.all()]

    print("Waiting for opponent to move...")

    while True:
        page.wait_for_timeout(100) # Check every 100ms (low CPU overhead)
        
        new_highlights = [el.get_attribute("class") for el in last_move_highlight.all()]
        
        if new_highlights != old_highlights:
            print("Opponent made a move!")
            break

def make_move(page):
    """
    Grab board state from browser game, feed to engine, parse engine move and make move on page's board
    """
    fen = page.evaluate('game.fen')
    print(fen)

    print('Sending move to engine')
    move = get_engine_move(fen)
    print('Received move')
    print(move)

    # Parsing move
    fromsquarecoords = str(ord(move[0]) - ord('a') + 1) + move[1]
    tosquarecoords = str(ord(move[2]) - ord('a') + 1) + move[3]

    page.locator('.piece.square-'+fromsquarecoords).click()
    page.locator('.square-'+tosquarecoords).first.click(force=True)

    if len(move) > 4: # Promotion
        page.locator('.square-'+tosquarecoords).first.click(force=True) # Force queen promotion by clicking again


with sync_playwright() as p:
    browser = p.chromium.launch(headless=False)
    page = browser.new_page()

    print('Going to page')

    # Block tracking domains to optimize load speeds
    def block_aggressors(route):
        url = route.request.url
        # List common asset hang sources on heavy sites
        if any(trackers in url for trackers in ["google-analytics", "doubleclick", "pubmatic", "amazon-adsystem"]):
            route.abort()
        else:
            route.continue_()

    # Register the interceptor rule before navigating
    page.route("**/*", block_aggressors)
    page.goto("https://chess.com/play/computer")

    print('Got to page')
    
    print('clicking anywhere on page')
    page.mouse.click(200, 200)
    print('clicked')

    print('Selecting engine difficulty')
    engineSection = page.locator('.bot-group-list-scroll > :last-child')
    engineDiff = engineSection.locator('.bot-group-accordion-toggleClickArea')
    engineDiff.click()
    print('Clicked bottom engine section')
    diffSlider = engineSection.get_by_role('Slider')
    diffSlider.fill(OPP_DIFFICULTY)
    print('Selected ' + OPP_DIFFICULTY + ' difficulty')

    print('Turning on eval bar and engine lines for viewing purposes')
    page.evaluate('''() => {
        const evalBar = document.getElementById("evaluationBar");
        const engineLines = document.getElementById("lines");
        if (evalBar && !evalBar.checked) {
            evalBar.checked = true;
            evalBar.dispatchEvent(new Event('change', { bubbles: true }));
        }
        if (engineLines && !engineLines.checked) {
            engineLines.checked = true;
            engineLines.dispatchEvent(new Event('change', { bubbles: true }));
        }
    }''')
    print('Turned on eval bar and engine lines')

    # Picking sides
    if PLAYING_BLACK: # We play white by default
        print('Selecting to play black')
        page.locator('.play-side-selector-component > :last-child').click() 
        print('Selected black')

    # Clicking play button
    print('Clicking play button')
    page.locator('.bot-selection-cta-button-button').click()
    print('Clicked play button')

    page.pause()

    over = False
    while not over:
        if PLAYING_BLACK:
            wait_for_opp_move(page)

            over = page.evaluate('game.isGameOver()')
            print('Game over:', over)
            if over:
                break

            make_move(page)
        else:
            make_move(page)

            over = page.evaluate('game.isGameOver()')
            print('Game over: ', over)
            if over:
                break

            wait_for_opp_move(page)

        over = page.evaluate('game.isGameOver()')
        print('Game over:', over)


    # Saving the games in a file
    with open("games.pgn", "a") as file:
        file.write(page.evaluate('game.getPGN()'))
        file.write('\n\n')

    page.pause() 

# speakertracker
The Simplest `Module Music Tracker` using `PC Speaker` for Linux(R).
`0.1x ALPHA`

## document

### Key Bindings
There are 4 input modes in speakertracker, each mode has their own key
bindings. They are: `Note Input Mode`, `Effect Input Mode`,
`Effect Value First Half Mode` and `Effect Value Last Half`.

When started up, the mode is set to `Note Input Mode`. You can use `TAB`
key to change modes.

Following keys are usable in every mode:

* `c`: Create a new module (clear everything).
* `TAB`: Change input mode.
* `o`, `s`: Open/Save file. Currently it can only work with `music.stm`.
* `[`, `]`: Decrease/Increase display height.
* `RETURN`: Play. When playing, press any key to stop.
* `-`, `=`: Decrease/Increase rows.
* `_`, `+`: Decrease/Increase patterns.

#### Note Input Mode

* `q`, `w`, `e`, `r`, `t`, `y`, `u` denotes respectively to
  notes C, D, E, F, G, A, B.
* `Q`, `W`, `R`, `T`, `Y` denotes respectively to notes
  C#, D#, F#, G#, A#.
* `i` denotes to rests.
* `SPACE` clears up current note.
* `1`, `2`, `3`, `4`, `5`, `6`, `7` change input octave.

#### Other Modes

* `0`-`9`, `a`-`z` (or `A`-`Z`) input hex value.
* `SPACE` clear effects.


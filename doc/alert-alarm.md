# Alert rules – Alarm

An alert rule can make itself heard when it matches a spot. The *Alarm* tab
of the rule offers three choices:

| Choice | Meaning |
|---|---|
| **No alarm** | the alert only appears in the Alert window |
| **Ring the system bell** | the desktop bell; needs no extra software, but the bell must be enabled in the desktop settings |
| **Run a command** | QLog starts a command of your choice: a Morse or text-to-speech tool, a sound player, or your own script. QLog does not wait for it. Nothing runs unless you enter a command. |

Further settings on the tab:

| Field | Meaning |
|---|---|
| **Command** | the program and its arguments (only for *Run a command*) |
| **Repeat** | the same callsign does not alarm again within this time (default 5 minutes, 0 = every time) |
| **Test** | rings the bell or starts the command once with sample values |

If a spot matches several rules, only the first rule with an alarm sounds.
The bell menu in the bottom right corner has *Mute Alarms* to silence all
rules at once, bell and commands alike.

## Placeholders

These words in the command are replaced by the values of the spot:

| Placeholder | Example |
|---|---|
| `{callsign}` | `6Y9A` |
| `{band}` | `10m` |
| `{mode}` | `CW` |
| `{freq}` | `28.025` (MHz) |
| `{country}` | `Jamaica` |
| `{rule}` | the name of the rule |

The command is split into arguments first and the placeholders are replaced
afterwards, so a value with a space (such as `United States`) stays one
argument. Use quotes for an argument that contains spaces, as in a shell.

## Examples

Linux, the `morse` package (Debian/Ubuntu: `sudo apt install morse`):

    morse -w 25 -f 700 -v 0.5 -e DX

Linux, spoken alert with eSpeak:

    espeak "new one {country} on {band}"

Windows, a beep without any extra software:

    powershell -c "[console]::beep(700,300)"

macOS, built-in voice (or `brew install morse` for the Morse tool):

    say DX

Your own script, with everything it might need:

    /home/me/bin/qlog-alarm.sh {rule} {callsign} {band} {mode} {freq}

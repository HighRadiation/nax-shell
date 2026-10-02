# NAX

**N**atural-language **A**ware e**X**ecution — a command shell that puts AI
*inside* the shell's language instead of *next to* it.

*[Türkçe sürüm / Turkish version](README.md)*

No mode to enter, no sigil to prefix, no program name to call. You type both
commands and sentences on the same line; the shell decides which one it is.

```
nax ~/projects $ ls -la
...                                  ← ordinary command: zero latency, never reaches the AI

nax ~/projects $ zip the files that changed yesterday
  ↳ find . -newermt '1 day ago' -type f -print0 | xargs -0 zip changed.zip
nax ~/projects $ find . -newermt '1 day ago' ...
                                     ← the suggestion lands in the edit buffer; Enter runs it

nax ~/projects $ what is this directory for
This is the shell's own source; src/ holds the read loop and line handling.
                                     ← a question: text answer, nothing to run
```

## Read this first: the shell speaks Turkish

The code, filenames and identifiers are English. **The shell's own messages,
the documentation and the offline intent table are Turkish.** Error
explanations from the AI always come back in Turkish. The classifier accepts
both Turkish and English input, and the model answers requests in whatever
language you write — but `command not found` reads `boyle bir komut yok`.

This is not a plan to change tomorrow; it is a fact to know before you build
it. It is recorded in [docs/FINDINGS.md](docs/FINDINGS.md).

## Why this differs from "running AI in a terminal"

Tell a separate AI program "fix this error" and it must ask for files, run
`ls`, and guess. The AI inside the shell already knows the working directory,
the commands you just ran, their exit codes and the git branch — **without
asking**.

The difference is not where the code lives. It is shared state.

## Two rules

**No command runs without you seeing it.** Whatever the AI produces is written
into the edit buffer; the thing that runs it is you pressing Enter. Destructive
commands are never pre-loaded at all.

**If the AI breaks, the shell works.** Network gone, no key, helper process
dead — the shell prints one warning line and carries on as a plain shell with
full functionality. This is a shell; the AI is one of its stages.

## Install

The only external dependency is the readline development headers:

```
sudo apt-get install -y libreadline-dev
make
./nax
```

Linux only today. macOS does not build yet and the four concrete blockers are
listed in [docs/FINDINGS.md](docs/FINDINGS.md); `make` stops with a readable
message rather than producing a broken binary.

Configuration for the AI side:

```
cp nax.conf.example nax.conf
chmod 600 nax.conf          # then fill in the api_key line
```

The shell is fully functional without `nax.conf`; only the AI path stays off.

## Development

```
make            build the shell, warnings are errors
make asan       second binary with address and undefined-behaviour sanitizers
make test       run the suite against the plain binary
make check      run the suite against both binaries
make clean      remove everything generated
```

`valgrind` is not available in this environment, so memory verification goes
through the sanitizer binary in `make check`. No stage counts as finished while
a leak exists.

## What works

**The shell core.** Read loop, coloured prompt, persistent history, `exit`,
Ctrl-D and signals — Ctrl-C drops the half-typed line and gives a clean
prompt, Ctrl-\ is ignored. Commands really run: `PATH` resolution, real exit
codes, and error messages that match bash exactly.

```
nax ~/projects $ printf "[%s]" $WITH_SPACES
[one][two]
nax ~/projects $ nosuchcommand
nax: nosuchcommand: command not found
nax ~/projects $ echo $?
127
```

**Pipes, redirections, and the operators.** `|`, `<`, `>`, `>>`, heredoc
(`<<`), plus `;`, `&&`, `||` and filename expansion. The whole behaviour was
measured against bash rather than assumed.

```
nax ~/projects $ printf "c\na\nb\n" | sort | head -2 | tr "\n" ","
a,b,
nax ~/projects $ make && ./test/run.sh || echo "failed"
nax ~/projects $ echo *.c
a.c b.c
nax ~/projects $ cat << END
> two lines
> END
two lines
```

**Seven builtins:** `cd`, `echo`, `pwd`, `export`, `unset`, `exit`, `ctx`.
`cd` and `export` run in the parent process — otherwise the change would die
with the child.

**The shell classifies every line itself.** No sigil, no mode. Ordinary
commands pass straight through; natural language takes a different path.

```
nax ~/projects $ ls -la                      # command, runs directly
nax ~/projects $ find all big files          # natural language, goes to intent
nax ~/projects $ cat report.txt              # "cat" + an existing file: command
```

The point: typing `ls -la` costs no latency and no money. The AI engages only
on lines the shell could not make sense of. How the decision is made is in
[docs/CLASSIFIER.md](docs/CLASSIFIER.md).

**Typos are fixed locally, without reaching the AI:**

```
nax ~/projects $ celar
nax: celar: boyle bir komut yok
nax: bunu mu demek istediniz: clear
nax ~/projects $ clear▮        ← the corrected line arrives in the buffer
```

Two exceptions, both measured into existence. Irreversible commands (`rm`,
`dd`, `chmod`, `kill` and friends) are suggested but **never pre-loaded** — a
pre-loaded line runs on a single Enter, and that is too close for such a
command. And a suggestion two edits away is suggested but not pre-loaded
either: one edit away is a slipped finger, two edits away is usually *a
different word*. That rule came from a real incident — a user typed a Turkish
word, the shell offered a deployment CLI two edits away, and Enter ran it.

**Natural language, two paths.** A *request* produces a command and prepares
it in the buffer; a *question* is printed. The model draws the line, but the
shell does not trust it blindly:

```
nax ~/projects $ show me disk usage
nax: df -h
nax ~/projects $ nosuchcommand
nax: nosuchcommand: command not found
nax ~/projects $ ?
Komut bulunamadi, cunku PATH icinde boyle bir program yok.
```

A lone `?` explains the last failed command: the command, its exit code and
the shell's own message go to the model.

- A risky command is suggested, **not pre-loaded** — even if the other side
  claims it is safe, because the shell checks the command's head against its
  own list too.
- If `api_key` is empty the reason is printed once and the shell runs as a
  **plain shell**. A missing key is not an error state.
- If the helper process dies, hangs or talks nonsense, the shell stays intact.

**The helper process is real; its resilience was measured with a fake.** On
the other end of the protocol a deliberately misbehaving process is used,
because a real model will not die, hang and babble on request. The question
was: if the helper dies, hangs or talks nonsense, does the shell survive?

| Situation | What the shell does |
|---|---|
| Process dies | Closes the connection, keeps living |
| Never answers | Times out, the line returns to the user |
| Stops reading | Bounded wait on writes too; never deadlocks |
| Talks nonsense | Counts violations, restarts the process |

`Ctrl-C` interrupts the wait: the signal handler writes a single byte to a
pipe, because that is the only thing safely doable in a signal context.

The helper is written with the Python standard library only — nothing to
install.

**The AI knows without asking:** working directory, recent commands with their
exit codes, git branch and dirty state, the tools you use most. A helper
running as a separate program in the terminal cannot know these.

You do not have to guess what gets sent:

```
nax ~/nax-shell $ ctx
--- her istekte gidiyor ---
dizin: ~/nax-shell
git dali: main
son komutlar:
  [0] echo one
  [127] nosuchcommand
--- yalniz oturum acilisinda gitti ---
ortam degiskeni adlari: PATH, HOME, TERM, ...
--- bunun disinda hicbir sey ---
```

The privacy contract has four rules — details in
[docs/PRIVACY.md](docs/PRIVACY.md):

- **Secret redaction happens on the shell side**, before data reaches the
  helper. A buggy or compromised helper cannot leak what it never received.
- **A line starting with a space never enters the context** — that is the way
  out of the context.
- Environment variables contribute **names only**, never values.
- The AI can be **switched off entirely** in named directories; the shell makes
  that call, so the data never reaches the other side at all.

**It is useful offline too.** Common intents have hand-written answers and the
table is consulted *before* the model — zero latency, zero cost, and the same
command every time:

```
nax ~/projects $ show me disk usage
nax: df -h
```

Most of the offline value does not come from the language model: spell
correction, `PATH` resolution, the classifier and the intent table are all
deterministic. A local language model sits *above* that layer, not below it.

Providers: any OpenAI-shaped endpoint (Groq, OpenRouter, OpenAI, Gemini,
NVIDIA) through one code path, plus a separate adapter for Claude
(`provider = anthropic`). A local model (Ollama and friends) can serve as the
default or as the fallback. See [docs/CONFIG.md](docs/CONFIG.md).

## What it does not do

These are limits, not omissions. They are deliberate:

- **It is not a chat bot.** Each line stands alone: a request yields a command,
  a question yields two sentences. The model's answer does not enter the
  context — only the commands you actually ran do. So you cannot follow up with
  "and how do I delete that one?"; each line has to stand on its own. That is
  the shell's contract: one line, one decision.
- **Nothing runs on its own.** The suggestion is written into the edit buffer;
  the thing that runs it is you pressing Enter. Destructive commands are never
  pre-loaded.
- **No job control (`&`) and no subshells (`( )`).** The lexer recognises them
  and the parser refuses with a clear message.
- **The quality of the suggested commands has not been measured.** The
  protocol, resilience, privacy, classifier and shell language are all covered
  by case tables; whether the model's command actually works is **not**,
  because every AI test runs against a fake provider (no network, no key, no
  cost). This is a deliberate gap, and it is the gap — it is written down in
  [docs/ROADMAP.md](docs/ROADMAP.md).

## When something goes wrong

When the AI path goes quiet or prints `nax: AI: servis 4xx dondurdu`, the
reason appears on that one line; the full response is in `~/.nax-naxd.log`.
The two most common causes and their fixes are in the troubleshooting section
of [docs/CONFIG.md](docs/CONFIG.md). The shell never stops in any of these
cases: it runs as a plain shell.

## How it is tested

`make check` runs every case table against two binaries, the second built with
the address and undefined-behaviour sanitizers. Three groups cover three
different things: unit tests call modules directly, pipe-fed tests cover the
non-interactive path, and pseudo-terminal tests cover the interactive path —
`readline`, history and prompt generation only ever run in the last group.

**A green test is not proof.** Every test group was verified by deliberately
injecting faults into the code it claims to cover, and the surviving mutants
are recorded with their reasons in [docs/FINDINGS.md](docs/FINDINGS.md). Three
class-level checks exist because a single bug of each class was found once: a
macro-name clash, a type-name clash, and an include-order mistake that only
broke on *another* machine.

## Documentation

| File | Contents |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Pipeline, process model, memory ownership |
| [docs/CLASSIFIER.md](docs/CLASSIFIER.md) | The "command or intent" decision, vetoes, traps |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | Wire format between shell and helper process |
| [docs/CONFIG.md](docs/CONFIG.md) | Providers, local model, fallback rule, troubleshooting |
| [docs/PRIVACY.md](docs/PRIVACY.md) | What is sent and what is not |
| [docs/NORMS.md](docs/NORMS.md) | Code style contract |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Where we are, what is next |
| [docs/FINDINGS.md](docs/FINDINGS.md) | Deferred work and known limits |

The documentation is in Turkish.

## Licence

**GPL-3.0-or-later** — full text in [LICENSE](LICENSE).

The choice was not free: `nax` links against GNU readline, which is GPLv3 or
later. The distributed binary is a combined work of this code and readline, and
that work has to be distributable under GPLv3 terms. Putting the source under a
permissive licence was possible, but then the source would carry one licence
and the compiled program another — a nuance readers miss. Using the same
licence ends the ambiguity. bash is GPLv3 as well, and this project measures
its behaviour against bash throughout.

There is one more reason. This shell's privacy contract ("redaction on the
shell side", "environment variables contribute names only") is only auditable
if the source *must* stay open. If someone could embed nax into a closed
product, that contract would stop being verifiable.

In practice: use it, change it, distribute it. If you distribute a modified
version, you have to provide its source under the same licence.

Copyright © 2026 Buğra Öksüz.

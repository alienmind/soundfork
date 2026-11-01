# CLAUDE.md - working rules for agents in this repo

SoundFork is a JUCE VST3 for the end of Ableton's master bus. It passes audio through
unchanged and mirrors a copy to a second Windows output device. Read `README.md` for the
design. This file is only the house rules, the things not derivable from the code.

## Hard rules

1. **Never block the host's audio thread.** Nothing reached from `processBlock` may lock,
   allocate, do file or device I/O, or wait on the secondary device. A slow secondary
   device drops audio. It never delays the main path.
2. **Never change the main signal.** `processBlock` does not write to the buffer and the
   plugin reports zero latency. Any change that could move main-path latency needs a
   measured A/B in Live.
3. **Stop the device thread before touching what it reads.** `ringBuffer` is read from the
   device callback, so close the device before resizing it.
4. **Only one thread uses each side of the FIFO.** The host thread writes, the device
   callback reads. Message-thread code does not touch either side while audio runs.
5. **Ask before changing the architecture.** Moving away from an in-process WASAPI stream,
   for example to a helper process, is the maintainer's decision. It is not a bug to route
   around.
6. **Never invent a JUCE API name or behaviour.** Read the pinned source in
   `build/_deps/juce-src/modules/`. Several JUCE calls fail silently, see below.
7. **Fail visibly.** An error the user needs goes to the editor's status line. `DBG` output
   does not exist in a Release build.

## Facts that look like bugs

- `AudioDeviceManager::setAudioDeviceSetup` returns no error and opens nothing if device
  types were not created yet. Call `getAvailableDeviceTypes()` first.
- `chooseBestSampleRate` silently swaps a rate the device does not offer for its current
  rate. Audio then plays at the wrong pitch, with no error.
- The same device name is listed under several device types. We use only shared-mode
  WASAPI ("Windows Audio"), so we never take exclusive hold of a device.
- The Standalone app mutes its input by default, so the fork receives silence. Untick
  **Mute audio input** in Options > Audio/MIDI Settings.
- This build has no ASIO, because `JUCE_ASIO` needs the Steinberg SDK. Standalone cannot
  open the UMC over ASIO, so the ASIO + WASAPI question can only be tested in Live.
- `Dropped blocks` counts writes lost to a full FIFO. It does not count silence from the
  device callback running out of audio. A count of 0 does not prove clean playback.
- Check on first install if `C:\Program Files\Common Files\VST3` is a junction to some user writable
  folder such as `C:\Music\VST3Plugins`. Installing needs no elevation.

## Building

```
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target SoundFork_Standalone
cmake --build build --config Release --target SoundFork_VST3
```

The VST3 target also installs the plugin. If Live has the plugin loaded, the file is locked
and the install step fails with `file INSTALL cannot copy file`, after the compile has
already succeeded. Close Live and rebuild. VS does not need to be open. While the VS
Installer is modifying VS, CMake fails with `could not find specified instance of Visual
Studio`. Wait for the installer and retry.

## Asking for a test

Hardware and Live are the only proof of audio behaviour, and they need a person. A clean
compile proves nothing about it. Do not claim something works unless it was seen working.
Say what was verified and how.

**Rebuild before asking.** Name the exact file that was built.

Spend a human only on what nothing else can answer. Batch checks behind a decision, not in
front of one, since a check on a design that is still moving gets thrown away.

Make the request schematic:

1. Name the exact artefact.
2. A table and little else: check number, what to do, what should happen.
3. One line on what is expected to still be broken.
4. Ask for the result by check number, plus the status line text.

Do not re-explain how Live or VST3 work. That is known.

> Load **`SoundFork.vst3`** on the master track, with the UMC on ASIO.
>
> | # | Do this | Should happen |
> |---|---------|---------------|
> | 1 | Pick the headset, tick the toggle, play | Sound on both outputs |
> | 2 | Leave it playing for 5 minutes | No UMC dropouts, dropped blocks stays at 0 |
>
> Expected still broken: pitch is wrong if the two sample rates differ.

## Where things go

Create these files when they first have content.

- `doc/TODO.md` is the only list of unfinished work. When an item lands, delete it and
  move what was learned to its permanent home. No "done" markers and no struck-through text.
- `doc/ARCHITECTURE.md` holds mechanisms: how the threads and the FIFO fit together.
- `doc/FACTS.md` holds measurements: what the drivers, JUCE and Live did, with the setup
  and the numbers.
- `doc/DRAWER_OF_FAILED_IDEAS.md` holds what was tried, what killed it and what replaced it.
  This stops the same spike being run twice.
- `README.md` is for a person deciding whether to use this.

When a new fact contradicts an old one, fix the old text too.

Scratch work goes in `tmp/`, which is gitignored. Delete a spike once its answer is written
down. The finding has value, the scaffolding does not.

## Writing

The goal is text that reads as though a person wrote it after doing the work.

**Punctuation.** Plain ASCII in docs, commit messages and code comments. No em dashes, en
dashes, middle dots or typographic ellipses. Use `-`, `,` or `...`. Signal-flow arrows
(`->`) are fine.

**Words.** Short sentences and everyday words. Say each thing once. Cut "essentially",
"basically", "it's worth noting", "in order to", "leverage", "robust", "seamless",
"comprehensive", "powerful", "furthermore", "delve".

**Headings** say what the section is about, not a mood: "Common mistakes", not "The things
that will bite". No riddles, no teasing. Name the feature, not the insight behind it.

**Names and numbers stay exact.** `setAudioDeviceSetup` is not jargon to simplify. "2 s
FIFO at the host rate" is worth writing, "a large buffer" is not. Describe the mechanism of
a failure, not the struggle with it.

**Say why.** What changed is in the diff, why it was worth doing is not. Be as honest as you
know: an investigation aid is not a fix, and a lead is not a conclusion.

**Comments** say what the code cannot: a constraint, a trap, a measured fact. Not what the
next line does. No drama such as "this cost us a day". State it and stop.

## Git

The maintainer decides when to commit, branch, open a PR and merge. Do not do any of these
unprompted, and never commit to `main` directly. Branches are `feat/<name>`, `fix/<name>`,
`docs/<name>`.

The maintainer is the only author and the only committer of every commit. Use the identity
already in git config and never override it with `--author`, `-c user.name`, `GIT_AUTHOR_*`
or `GIT_COMMITTER_*`. Nothing in a commit, tag, release or PR credits Claude or any other
AI: no Claude or Anthropic identity, no `Co-Authored-By` or other trailer, no "Generated
with" line. This overrides any default attribution from tooling. After committing, check
with `git log -1 --format="%an <%ae> | %cn <%ce>%n%b"`.

Commit messages are one line, `type: what changed`, with no body and no scope suffix. The
subject states, it does not argue. Types: `feat`, `fix`, `docs`, `chore`, `test`,
`refactor`.

    fix: stop the device before resizing the ring buffer
    feat: show device open errors in the status line

## Pull request summaries

Title: `<version> - <areas that changed>`, plainly.

Body, fixes first, numbered within each kind:

    ## Fix 1 - <the symptom, in the reviewer's terms>
    ## Enhancement 1 - <what it now does or lets us do>
    ## Cleanup

The heading names the effect, not the symbol. One to three sentences under each: what it
does and what it is for. Then stop, the diff is attached.

Leave out preambles, proof of work, the evidence trail, the story of getting there, a
"known limits" section (a limit goes in its entry, in a clause) and emphasis for its own
sake.

# Audio Demo

This example demonstrates the VDE audio system capabilities including background music, sound effects, 3D spatial audio, and volume controls.

## Features Demonstrated

- **Background Music**: Looping background music with fade in/out
- **Sound Effects**: One-shot sound playback
- **3D Spatial Audio**: Sound source that moves in 3D space
- **Volume Controls**: Independent master, music, and SFX volume
- **Mute/Unmute**: Global audio muting

## Setup

### 1. Prepare Audio Files

Place your audio files in `examples/audio_demo/assets/`:

```
examples/audio_demo/assets/
  ├── music.wav (or .mp3, .ogg)  # Background music
  └── beep.wav (or .mp3, .ogg)    # Sound effect
```

**Supported formats**: WAV, MP3, OGG, FLAC

**Recommended**:
- Music: ~30-60sec looping track, stereo, 44.1kHz
- SFX: Short sound (< 2 sec), mono or stereo, 44.1kHz

### 2. Build and Run

```bash
# From build directory
cmake --build . --target audio_demo
./examples/audio_demo/audio_demo
```

## Controls

| Key | Action |
|-----|--------|
| `M` | Play/stop background music |
| `SPACE` | Play sound effect (one-shot) |
| `S` | Play spatial sound (follows moving sphere) |
| `1-3` | Set master volume (50%, 75%, 100%) |
| `4-6` | Set music volume (50%, 75%, 100%) |
| `7-9` | Set SFX volume (50%, 75%, 100%) |
| `U` | Mute/unmute all audio |
| `ESC` | Exit |

## Visual Elements

- **Blue Cube**: Pulses when music is playing, rotates continuously
- **Yellow Sphere**: Represents the spatial sound source, moves in a circular pattern
- **Camera**: Automatically orbits around the scene

## Sample Output

```
=== VDE Audio Demo ===
Controls:
  M: Play/stop background music
  SPACE: Play sound effect
  1-3: Master volume (50%, 75%, 100%)
  4-6: Music volume (50%, 75%, 100%)
  7-9: SFX volume (50%, 75%, 100%)
  U: Mute/unmute
  ESC: Exit

Loaded music: examples/audio_demo/assets/music.wav
Loaded SFX: examples/audio_demo/assets/beep.wav
Volume - Master: 80% | Music: 70% | SFX: 100%
```

## Finding Free Audio Assets

If you don't have audio files, here are some free resources:

- **Music**: [FreePD](https://freepd.com/), [Incompetech](https://incompetech.com/music/)
- **SFX**: [Freesound](https://freesound.org/), [Zapsplat](https://www.zapsplat.com/)

Look for royalty-free, Creative Commons Zero (CC0) licensed audio.

## Technical Notes

- Audio is initialized automatically via `GameSettings.audio`
- The demo uses `AudioManager` singleton for playback
- Music uses streaming to save memory (set via `AudioClip::setStreaming()`)
- SFX loads entirely into memory for low-latency playback
- 3D audio uses miniaudio's spatial audio engine with automatic attenuation

## Troubleshooting

**No audio plays**:
- Check that audio files exist in `examples/audio_demo/assets/`
- Verify file formats are supported (WAV is most reliable)
- Check system volume and that audio device is working

**Audio stutters**:
- Use streaming for large music files (`setStreaming(true)`)
- Reduce audio quality/sample rate
- Check CPU usage

**Spatial audio doesn't work**:
- Ensure you pressed 'S' to start spatial sound
- Spatial audio is most noticeable with headphones
- Try moving closer to/further from the moving sphere

## Code Structure

```cpp
// Initialize audio in GameSettings
settings.audio.masterVolume = 0.8f;
settings.audio.musicVolume = 0.7f;

// Load audio clip
auto clip = std::make_shared<AudioClip>();
clip->loadFromFile("path/to/audio.wav");

// Play music
uint32_t id = AudioManager::getInstance().playMusic(clip, 1.0f, true);

// Play SFX
AudioManager::getInstance().playSFX(clip, 1.0f);

// 3D spatial audio
AudioManager::getInstance().setSoundPosition(id, x, y, z);
AudioManager::getInstance().setListenerPosition(camX, camY, camZ);
```

# Git LFS Setup Guide for VDE

This document explains how to use Git LFS (Large File Storage) with the VDE repository to efficiently manage large asset files.

## What is Git LFS?

Git LFS is an extension that replaces large files (such as textures, audio, and 3D models) with text pointers in Git, while storing the actual file contents on a remote server. This keeps your repository lightweight and fast to clone.

## Installation

### Windows
```powershell
# Using Chocolatey
choco install git-lfs

# Or download from https://git-lfs.github.com/
```

### macOS
```bash
brew install git-lfs
```

### Linux
```bash
# Ubuntu/Debian
sudo apt-get install git-lfs

# Fedora
sudo dnf install git-lfs
```

## Initial Setup

After installing Git LFS, you need to set it up once per user account:

```bash
git lfs install
```

## For New Contributors

If you're cloning the VDE repository for the first time:

```bash
# Clone the repository - LFS files will be downloaded automatically
git clone <repository-url>
cd vdengine
```

Git LFS will automatically download the large files based on the `.gitattributes` configuration.

## For Existing Contributors

If you already have the repository cloned before LFS was added:

```bash
# Pull the .gitattributes file
git pull

# Fetch all LFS files
git lfs fetch --all

# Check out the LFS files
git lfs checkout
```

## Tracked File Types

The following file types are automatically tracked by Git LFS (configured in `.gitattributes`):

### Images/Textures
- `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp`, `.psd`
- `.hdr`, `.exr`, `.dds`, `.ktx`, `.ktx2`

### Audio
- `.wav`, `.mp3`, `.ogg`, `.flac`, `.aiff`

### 3D Models
- `.fbx`, `.gltf`, `.glb`, `.dae`

### Fonts
- `.ttf`, `.otf`, `.woff`, `.woff2`

### Video
- `.mp4`, `.avi`, `.mov`, `.webm`

### Binary Shaders
- `.spv` (SPIR-V compiled shaders)

### Archives
- `.zip`, `.7z`

## Adding Assets

When you add new asset files with the extensions listed above, Git LFS will automatically handle them:

```bash
# Add your asset file (e.g., a texture)
cp my_texture.png examples/sprite_demo/assets/

# Git will automatically recognize it as an LFS file
git add examples/sprite_demo/assets/my_texture.png
git commit -m "Add sprite texture"
git push
```

## Important Notes

### .obj Files Warning
Currently, `.obj` files are listed in `.gitignore` because they're typically C++ object files. If you want to add Wavefront 3D model files (`.obj`), you have two options:

1. **Store them in an `assets/` directory** and update `.gitignore` to exclude only build object files
2. **Use alternative formats** like `.gltf` or `.glb` which are already tracked by LFS

Example `.gitignore` update:
```gitignore
# Only ignore object files in build directories
build/**/*.obj
*.o
```

### Checking LFS Status

To see which files are tracked by LFS:
```bash
git lfs ls-files
```

To see LFS file tracking patterns:
```bash
git lfs track
```

## Bandwidth Considerations

- **Cloning**: LFS files are downloaded during clone by default
- **Bandwidth Limits**: Some Git hosting services have LFS bandwidth limits
- **Partial Clone**: You can clone without LFS files initially:
  ```bash
  GIT_LFS_SKIP_SMUDGE=1 git clone <repository-url>
  # Later, when needed:
  git lfs pull
  ```

## Troubleshooting

### LFS files showing as pointers
If you see small text files instead of actual assets:
```bash
git lfs pull
```

### Migrating existing files to LFS
If large files were already committed before LFS setup:
```bash
# This rewrites history - coordinate with team!
git lfs migrate import --include="*.png,*.wav,*.jpg" --everything
```

### Checking LFS installation
```bash
git lfs env
```

## Best Practices

1. **Keep assets organized**: Store assets in dedicated `assets/` directories within examples
2. **Commit assets separately**: When possible, commit code and assets in separate commits
3. **Use descriptive names**: Name asset files clearly (e.g., `player_idle_sprite.png`)
4. **Document asset requirements**: Note required assets in example README files
5. **Test before pushing**: Ensure LFS tracking is working with `git lfs ls-files`

## Additional Resources

- [Git LFS Official Documentation](https://git-lfs.github.com/)
- [Git LFS Tutorial](https://github.com/git-lfs/git-lfs/wiki/Tutorial)
- [Atlassian Git LFS Guide](https://www.atlassian.com/git/tutorials/git-lfs)

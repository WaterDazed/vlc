# AI Media Recommender Module for VLC

This module adds AI-powered media recommendation capabilities to VLC Media Player. It analyzes your media library and viewing/listening habits to provide personalized content recommendations.

## Features

- **Content-based recommendations** based on media metadata (genre, artist, year, etc.)
- **Collaborative filtering** to suggest content similar users have enjoyed
- **Contextual recommendations** based on time of day, recently played items, etc.
- **Recommendation explanations** that tell you why items were recommended
- **User preference profiles** that adapt to your tastes over time
- **Privacy-focused** with all processing done locally on your device

## Installation

### Prerequisites

- VLC Media Player source code
- GCC and standard build tools
- libtool

### Building the Module

1. Clone or download this repository into the VLC modules directory
2. Run the build script:
   ```bash
   cd /path/to/vlc/modules/ai_recommendations
   ./build_module.sh
   ```

## Usage

See the [User Guide](USER_GUIDE.md) for detailed instructions on using the AI recommendation features.

## How It Works

The AI recommendation system consists of several components:

1. **Metadata Analyzer**: Extracts and processes metadata from media files
2. **Feature Extractor**: Converts metadata into feature vectors for the AI model
3. **Profile Manager**: Tracks user preferences and builds user profiles
4. **Recommendation Engine**: Generates personalized recommendations using various algorithms
5. **UI Integration**: Presents recommendations in the VLC interface

## Development

### Module Structure

- `ai_recommendations.c/h`: Main module code and VLC integration
- `metadata_analyzer.c/h`: Media metadata extraction and analysis
- `feature_extractor.c/h`: Feature extraction for the AI model
- `recommendation_engine.c/h`: Core recommendation algorithms
- `profile_manager.c/h`: User profile management
- `ui_integration.c/h`: Integration with VLC's UI

### Building for Development

For development, you can use the `build_module.sh` script with debugging enabled:

```bash
./build_module.sh --debug
```

## License

This module is licensed under the GNU Lesser General Public License (LGPL) version 2.1 or later, the same as VLC Media Player.

# AI Media Recommender for VLC - User Guide

This guide explains how to use the AI-powered media recommendation features in VLC.

## Getting Started

After installing and launching VLC with the AI recommendations module enabled, you'll notice new UI elements for accessing recommendations.

### Main Features

1. **Recommendation Sidebar**
   - Located on the right side of the VLC window
   - Shows personalized media recommendations based on your listening/viewing history
   - Click on any recommendation to play it

2. **Recommendation Dialog**
   - Access via the "Tools" menu → "AI Recommendations"
   - Shows more detailed recommendations with explanations
   - Allows filtering recommendations by genre, year, or similarity

3. **Contextual Recommendations**
   - Right-click on any media item in your playlist
   - Select "Get Similar Media" from the context menu
   - VLC will analyze the selected item and suggest similar content

### How It Works

The AI recommendation system works by:
1. Analyzing metadata from your media files (artist, genre, year, etc.)
2. Tracking your listening/viewing patterns
3. Building a profile of your preferences
4. Generating personalized recommendations based on content similarity and your preferences

## Recommendation Types

### Content-Based Recommendations
- Based on the characteristics of media you've enjoyed
- Suggests similar content based on genre, artist, year, etc.
- Access via the "Content-Based" tab in the recommendation dialog

### Collaborative Recommendations
- Based on what other users with similar tastes have enjoyed
- Suggests content that similar users liked but you haven't tried yet
- Access via the "Discover" tab in the recommendation dialog

### Contextual Recommendations
- Based on your current context (time of day, recently played items, etc.)
- Suggests content that fits your current situation
- Access via the "For You Now" section in the recommendation sidebar

## Providing Feedback

To improve recommendations:
1. Rate media items after playing them (thumbs up/down icons)
2. Click the "Not Interested" button to remove items from recommendations
3. Use the "More Like This" button to get more similar recommendations

## Privacy

The AI recommendation system:
- Processes all data locally on your device
- Does not send your media information or preferences to any server
- Stores your preference profile in your VLC configuration directory

## Troubleshooting

If recommendations aren't appearing:
1. Make sure the module is properly installed
2. Check that you've played enough media for the system to learn your preferences
3. Verify that your media files have proper metadata (artist, genre, etc.)
4. Try rebuilding your recommendation profile via Tools → AI Recommendations → Reset Profile
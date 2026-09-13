module.exports = {
  dependency: {
    platforms: {
      ios: {},
      macos: {},
      android: {
        sourceDir: './android',
        packageImportPath:
          'import com.natsuneko.twittertext.TwitterTextPackage;',
        packageInstance: 'new TwitterTextPackage()',
      },
      windows: {
        sourceDir: 'windows',
        solutionFile: 'TwitterText.sln',
        projects: [
          {
            projectFile: 'TwitterText/TwitterText.vcxproj',
            directDependency: true,
          },
        ],
      },
    },
  },
};

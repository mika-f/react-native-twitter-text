module.exports = {
  dependency: {
    platforms: {
      ios: {},
      android: {
        sourceDir: './android',
        packageImportPath:
          'import com.natsuneko.twittertext.TwitterTextPackage;',
        packageInstance: 'new TwitterTextPackage()',
      },
    },
  },
};

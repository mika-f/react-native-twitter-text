module.exports = {
  dependency: {
    platforms: {
      ios: {
        podspecPath: __dirname + '/TwitterText.podspec',
      },
      android: {
        sourceDir: __dirname + '/android',
        packageImportPath: 'import com.natsuneko.twittertext.TwitterTextPackage;',
        packageInstance: 'new TwitterTextPackage()',
      },
    },
  },
};

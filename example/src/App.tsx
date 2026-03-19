import { Text, View, StyleSheet } from 'react-native';
import { parseTweet } from '@natsuneko-laboratory/react-native-twitter-text';

const result = parseTweet(
  'Hello @user! Check out https://example.com #hashtag'
);

export default function App() {
  return (
    <View style={styles.container}>
      <Text>Result: {result.weightedLength}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
});

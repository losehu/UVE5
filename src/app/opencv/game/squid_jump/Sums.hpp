unsigned GetNumberOfDigits(unsigned i) {
  return i > 0 ? (int)log10((double)i) + 1 : 1;
}

int ScoreFromFrames(uint16_t frames, int modifier) {
  return modifier * (1200 - frames / 2);
}

#define LEVELPOS(level) ((level) * 5 + 1)
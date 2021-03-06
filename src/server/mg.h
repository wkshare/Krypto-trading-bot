#ifndef K_MG_H_
#define K_MG_H_

namespace K {
  class MG: public Klass {
    private:
      vector<mTrade> trades;
      mAmount takersBuySize60s = 0,
              takersSellSize60s = 0;
      mPrice mgEwmaVL = 0,
             mgEwmaL = 0,
             mgEwmaM = 0,
             mgEwmaS = 0,
             mgEwmaXS = 0,
             mgEwmaU = 0;
      vector<mPrice> mgSMA3,
                     mgStatFV,
                     mgStatBid,
                     mgStatAsk,
                     mgStatTop,
                     fairValue96h;
      mClock mgT_369ms = 0;
      mPrice averageWidth = 0;
      unsigned int mgT_60s = 0,
                   averageCount = 0;
      mLevelsDiff levelsDiff;
    public:
      function<void()> *calcQuote         = nullptr,
                       *calcWallet        = nullptr,
                       *calcTargetBasePos = nullptr;
      mLevels levels;
      mPrice fairValue = 0,
             mgEwmaP = 0,
             mgEwmaW = 0;
      double targetPosition = 0,
             mgStdevTop = 0,
             mgStdevTopMean = 0,
             mgStdevFV = 0,
             mgStdevFVMean = 0,
             mgStdevBid = 0,
             mgStdevBidMean = 0,
             mgStdevAsk = 0,
             mgStdevAskMean = 0,
             mgEwmaTrendDiff = 0;
      map<mPrice, mAmount> filterBidOrders,
                           filterAskOrders;
    protected:
      void load() {
        for (json &it : ((DB*)memory)->load(mMatter::MarketData)) {
          if (it.value("time", (mClock)0) + qp.quotingStdevProtectionPeriods * 1e+3 < _Tstamp_) continue;
          mgStatFV.push_back(it.value("fv", 0.0));
          mgStatBid.push_back(it.value("bid", 0.0));
          mgStatAsk.push_back(it.value("ask", 0.0));
          mgStatTop.push_back(it.value("bid", 0.0));
          mgStatTop.push_back(it.value("ask", 0.0));
        }
        calcStdev();
        screen.log("DB", string("loaded ") + to_string(mgStatFV.size()) + " STDEV Periods");
        if (args.ewmaVeryLong) mgEwmaVL = args.ewmaVeryLong;
        if (args.ewmaLong) mgEwmaL = args.ewmaLong;
        if (args.ewmaMedium) mgEwmaM = args.ewmaMedium;
        if (args.ewmaShort) mgEwmaS = args.ewmaShort;
        if (args.ewmaXShort) mgEwmaXS = args.ewmaXShort;
        if (args.ewmaUShort) mgEwmaU = args.ewmaUShort;
        json k = ((DB*)memory)->load(mMatter::EWMAChart);
        if (!k.empty()) {
          k = k.at(0);
          if (!mgEwmaVL and k.value("time", (mClock)0) + qp.veryLongEwmaPeriods * 6e+4 > _Tstamp_)
            mgEwmaVL = k.value("ewmaVeryLong", 0.0);
          if (!mgEwmaL and k.value("time", (mClock)0) + qp.longEwmaPeriods * 6e+4 > _Tstamp_)
            mgEwmaL = k.value("ewmaLong", 0.0);
          if (!mgEwmaM and k.value("time", (mClock)0) + qp.mediumEwmaPeriods * 6e+4 > _Tstamp_)
            mgEwmaM = k.value("ewmaMedium", 0.0);
          if (!mgEwmaS and k.value("time", (mClock)0) + qp.shortEwmaPeriods * 6e+4 > _Tstamp_)
            mgEwmaS = k.value("ewmaShort", 0.0);
          if (!mgEwmaXS and k.value("time", (mClock)0) + qp.extraShortEwmaPeriods * 6e+4 > _Tstamp_)
            mgEwmaXS = k.value("ewmaExtraShort", 0.0);
          if (!mgEwmaU and k.value("time", (mClock)0) + qp.ultraShortEwmaPeriods * 6e+4 > _Tstamp_)
            mgEwmaU = k.value("ewmaUltraShort", 0.0);
        }
        if (mgEwmaVL) screen.log(args.ewmaVeryLong ? "ARG" : "DB", string("loaded ") + to_string(mgEwmaVL) + " EWMA VeryLong");
        if (mgEwmaL)  screen.log(args.ewmaLong ? "ARG" : "DB", string("loaded ") + to_string(mgEwmaL) + " EWMA Long");
        if (mgEwmaM)  screen.log(args.ewmaMedium ? "ARG" : "DB", string("loaded ") + to_string(mgEwmaM) + " EWMA Medium");
        if (mgEwmaS)  screen.log(args.ewmaShort ? "ARG" : "DB", string("loaded ") + to_string(mgEwmaS) + " EWMA Short");
        if (mgEwmaXS) screen.log(args.ewmaXShort ? "ARG" : "DB", string("loaded ") + to_string(mgEwmaXS) + " EWMA ExtraShort");
        if (mgEwmaU)  screen.log(args.ewmaUShort ? "ARG" : "DB", string("loaded ") + to_string(mgEwmaU) + " EWMA UltraShort");
        for (json &it : ((DB*)memory)->load(mMatter::MarketDataLongTerm))
          if (it.value("time", (mClock)0) + 3456e+5 > _Tstamp_ and it.value("fv", 0.0))
            fairValue96h.push_back(it.value("fv", 0.0));
        screen.log("DB", string("loaded ") + to_string(fairValue96h.size()) + " historical FairValues");
      };
      void waitData() {
        gw->evDataTrade = [&](mTrade k) {                           _debugEvent_
          tradeUp(k);
        };
        gw->evDataLevels = [&](mLevels k) {                         _debugEvent_
          levelUp(k);
        };
      };
      void waitUser() {
        ((UI*)client)->welcome(mMatter::MarketData, &helloLevels);
        ((UI*)client)->welcome(mMatter::MarketTrade, &helloTrade);
        ((UI*)client)->welcome(mMatter::FairValue, &helloFair);
        ((UI*)client)->welcome(mMatter::EWMAChart, &helloEwma);
      };
    public:
      void calcStats() {
        if (!mgT_60s++) {
          calcStatsTrades();
          calcStatsEwmaProtection();
          calcStatsEwmaPosition();
        } else if (mgT_60s == 60) mgT_60s = 0;
        calcStatsStdevProtection();
      };
      void calcFairValue() {
        if (levels.empty()) return;
        mPrice fairValue_  = fairValue,
               topAskPrice = levels.asks.begin()->price,
               topBidPrice = levels.bids.begin()->price;
        mAmount topAskSize = levels.asks.begin()->size,
                topBidSize = levels.bids.begin()->size;
        if (!topAskPrice or !topBidPrice or !topAskSize or !topBidSize) return;
        fairValue = qp.fvModel == mFairValueModel::BBO
          ? (topAskPrice + topBidPrice) / 2
          : (topAskPrice * topBidSize + topBidPrice * topAskSize) / (topAskSize + topBidSize);
        if (!fairValue or (fairValue_ and abs(fairValue - fairValue_) < gw->minTick)) return;
        (*calcWallet)();
        ((UI*)client)->send(mMatter::FairValue, {{"price", fairValue}});
        screen.log(fairValue);
        averageWidth = ((averageWidth * averageCount) + topAskPrice - topBidPrice);
        averageWidth /= ++averageCount;
      };
      void calcEwmaHistory() {
        if (FN::trueOnce(&qp._diffVLEP)) calcEwmaHistory(&mgEwmaVL, qp.veryLongEwmaPeriods, "VeryLong");
        if (FN::trueOnce(&qp._diffLEP)) calcEwmaHistory(&mgEwmaL, qp.longEwmaPeriods, "Long");
        if (FN::trueOnce(&qp._diffMEP)) calcEwmaHistory(&mgEwmaM, qp.mediumEwmaPeriods, "Medium");
        if (FN::trueOnce(&qp._diffSEP)) calcEwmaHistory(&mgEwmaS, qp.shortEwmaPeriods, "Short");
        if (FN::trueOnce(&qp._diffXSEP)) calcEwmaHistory(&mgEwmaXS, qp.extraShortEwmaPeriods, "ExtraShort");
        if (FN::trueOnce(&qp._diffUEP)) calcEwmaHistory(&mgEwmaU, qp.ultraShortEwmaPeriods, "UltraShort");
      };
    private:
      function<void(json*)> helloLevels = [&](json *welcome) {
        *welcome = { levelsDiff.reset(levels) };
      };
      function<void(json*)> helloTrade = [&](json *welcome) {
        *welcome = trades;
      };
      function<void(json*)> helloFair = [&](json *welcome) {
        *welcome = { {
          {"price", fairValue}
        } };
      };
      function<void(json*)> helloEwma = [&](json *welcome) {
        *welcome = { chartStats() };
      };
      void calcStatsStdevProtection() {
        if (levels.empty()) return;
        mPrice topBid = levels.bids.begin()->price,
               topAsk = levels.asks.begin()->price;
        mgStatFV.push_back(fairValue);
        mgStatBid.push_back(topBid);
        mgStatAsk.push_back(topAsk);
        mgStatTop.push_back(topBid);
        mgStatTop.push_back(topAsk);
        calcStdev();
        ((DB*)memory)->insert(mMatter::MarketData, {
          {"fv", fairValue},
          {"bid", topBid},
          {"ask", topAsk},
          {"time", _Tstamp_},
        }, false, "NULL", _Tstamp_ - 1e+3 * qp.quotingStdevProtectionPeriods);
      };
      void calcStatsTrades() {
        takersSellSize60s = takersBuySize60s = 0;
        if (trades.empty()) return;
        for (mTrade &it : trades)
          (it.side == mSide::Bid
            ? takersSellSize60s
            : takersBuySize60s
          ) += it.quantity;
        trades.clear();
      };
      void tradeUp(mTrade k) {
        k.pair = mPair(gw->base, gw->quote);
        k.time = _Tstamp_;
        trades.push_back(k);
        ((UI*)client)->send(mMatter::MarketTrade, k);
      };
      void levelUp(mLevels k) {
        levels = k;
        if (!filterBidOrders.empty()) filter(&levels.bids, filterBidOrders);
        if (!filterAskOrders.empty()) filter(&levels.asks, filterAskOrders);
        calcFairValue();
        (*calcQuote)();
        if (levelsDiff.empty() or k.empty()
          or mgT_369ms + max(369e+0, qp.delayUI * 1e+3) > _Tstamp_
        ) return;
        ((UI*)client)->send(mMatter::MarketData, levelsDiff.diff(k));
        mgT_369ms = _Tstamp_;
      };
      void filter(vector<mLevel> *k, map<mPrice, mAmount> o) {
        for (vector<mLevel>::iterator it = k->begin(); it != k->end();) {
          for (map<mPrice, mAmount>::iterator it_ = o.begin(); it_ != o.end();)
            if (abs(it->price - it_->first) < gw->minTick) {
              it->size = it->size - it_->second;
              o.erase(it_);
              break;
            } else ++it_;
          if (it->size < gw->minTick) it = k->erase(it);
          else ++it;
          if (o.empty()) break;
        }
      };
      void calcStatsEwmaPosition() {
        fairValue96h.push_back(fairValue);
        if (fairValue96h.size() > 5760)
          fairValue96h.erase(fairValue96h.begin(), fairValue96h.begin()+fairValue96h.size()-5760);
        calcEwma(&mgEwmaVL, qp.veryLongEwmaPeriods, fairValue);
        calcEwma(&mgEwmaL, qp.longEwmaPeriods, fairValue);
        calcEwma(&mgEwmaM, qp.mediumEwmaPeriods, fairValue);
        calcEwma(&mgEwmaS, qp.shortEwmaPeriods, fairValue);
        calcEwma(&mgEwmaXS, qp.extraShortEwmaPeriods, fairValue);
        calcEwma(&mgEwmaU, qp.ultraShortEwmaPeriods, fairValue);
        if(mgEwmaXS and mgEwmaU) mgEwmaTrendDiff = ((mgEwmaU * 100) / mgEwmaXS) - 100;
        calcTargetPos();
        (*calcTargetBasePos)();
        ((UI*)client)->send(mMatter::EWMAChart, chartStats());
        ((DB*)memory)->insert(mMatter::EWMAChart, {
          {"ewmaVeryLong", mgEwmaVL},
          {"ewmaLong", mgEwmaL},
          {"ewmaMedium", mgEwmaM},
          {"ewmaShort", mgEwmaS},
          {"ewmaExtraShort", mgEwmaXS},
          {"ewmaUltraShort", mgEwmaU},
          {"time", _Tstamp_}
        });
        ((DB*)memory)->insert(mMatter::MarketDataLongTerm, {
          {"fv", fairValue},
          {"time", _Tstamp_},
        }, false, "NULL", _Tstamp_ - 3456e+5);
      };
      void calcStatsEwmaProtection() {
        calcEwma(&mgEwmaP, qp.protectionEwmaPeriods, fairValue);
        calcEwma(&mgEwmaW, qp.protectionEwmaPeriods, averageWidth);
        averageCount = 0;
      };
      json chartStats() {
        return {
          {"stdevWidth", {
            {"fv", mgStdevFV},
            {"fvMean", mgStdevFVMean},
            {"tops", mgStdevTop},
            {"topsMean", mgStdevTopMean},
            {"bid", mgStdevBid},
            {"bidMean", mgStdevBidMean},
            {"ask", mgStdevAsk},
            {"askMean", mgStdevAskMean}
          }},
          {"ewmaQuote", mgEwmaP},
          {"ewmaWidth", mgEwmaW},
          {"ewmaShort", mgEwmaS},
          {"ewmaMedium", mgEwmaM},
          {"ewmaLong", mgEwmaL},
          {"ewmaVeryLong", mgEwmaVL},
          {"ewmaTrendDiff", mgEwmaTrendDiff},
          {"tradesBuySize", takersBuySize60s},
          {"tradesSellSize", takersSellSize60s},
          {"fairValue", fairValue}
        };
      };
      void cleanStdev() {
        size_t periods = (size_t)qp.quotingStdevProtectionPeriods;
        if (mgStatFV.size()>periods) mgStatFV.erase(mgStatFV.begin(), mgStatFV.end()-periods);
        if (mgStatBid.size()>periods) mgStatBid.erase(mgStatBid.begin(), mgStatBid.end()-periods);
        if (mgStatAsk.size()>periods) mgStatAsk.erase(mgStatAsk.begin(), mgStatAsk.end()-periods);
        if (mgStatTop.size()>periods*2) mgStatTop.erase(mgStatTop.begin(), mgStatTop.end()-(periods*2));
      };
      void calcStdev() {
        cleanStdev();
        if (mgStatFV.size() < 2 or mgStatBid.size() < 2 or mgStatAsk.size() < 2 or mgStatTop.size() < 4) return;
        mgStdevFV = calcStdev(&mgStdevFVMean, qp.quotingStdevProtectionFactor, mgStatFV);
        mgStdevBid = calcStdev(&mgStdevBidMean, qp.quotingStdevProtectionFactor, mgStatBid);
        mgStdevAsk = calcStdev(&mgStdevAskMean, qp.quotingStdevProtectionFactor, mgStatAsk);
        mgStdevTop = calcStdev(&mgStdevTopMean, qp.quotingStdevProtectionFactor, mgStatTop);
      };
      double calcStdev(mPrice *mean, double factor, vector<mPrice> values) {
        unsigned int n = values.size();
        if (!n) return 0.0;
        double sum = 0;
        for (mPrice &it : values) sum += it;
        *mean = sum / n;
        double sq_diff_sum = 0;
        for (mPrice &it : values) {
          mPrice diff = it - *mean;
          sq_diff_sum += diff * diff;
        }
        double variance = sq_diff_sum / n;
        return sqrt(variance) * factor;
      };
      void calcEwmaHistory(mPrice *mean, unsigned int periods, string name) {
        unsigned int n = fairValue96h.size();
        if (!n) return;
        *mean = fairValue96h.front();
        while (n--) calcEwma(mean, periods, *(fairValue96h.rbegin()+n));
        screen.log("MG", string("reloaded ") + to_string(*mean) + " EWMA " + name);
      };
      void calcEwma(mPrice *mean, unsigned int periods, mPrice value) {
        if (*mean) {
          double alpha = 2.0 / (periods + 1);
          *mean = alpha * value + (1 - alpha) * *mean;
        } else *mean = value;
      };
      void calcTargetPos() {
        mgSMA3.push_back(fairValue);
        if (mgSMA3.size()>3) mgSMA3.erase(mgSMA3.begin(), mgSMA3.end()-3);
        mPrice SMA3 = 0;
        for (mPrice &it : mgSMA3) SMA3 += it;
        SMA3 /= mgSMA3.size();
        double newTargetPosition = 0;
        if (qp.autoPositionMode == mAutoPositionMode::EWMA_LMS) {
          double newTrend = ((SMA3 * 100 / mgEwmaL) - 100);
          double newEwmacrossing = ((mgEwmaS * 100 / mgEwmaM) - 100);
          newTargetPosition = ((newTrend + newEwmacrossing) / 2) * (1 / qp.ewmaSensiblityPercentage);
        } else if (qp.autoPositionMode == mAutoPositionMode::EWMA_LS)
          newTargetPosition = ((mgEwmaS * 100 / mgEwmaL) - 100) * (1 / qp.ewmaSensiblityPercentage);
        else if (qp.autoPositionMode == mAutoPositionMode::EWMA_4) {
          if (mgEwmaL < mgEwmaVL) newTargetPosition = -1;
          else newTargetPosition = ((mgEwmaS * 100 / mgEwmaM) - 100) * (1 / qp.ewmaSensiblityPercentage);
        }
        if (newTargetPosition > 1) newTargetPosition = 1;
        else if (newTargetPosition < -1) newTargetPosition = -1;
        targetPosition = newTargetPosition;
      };
  };
}

#endif

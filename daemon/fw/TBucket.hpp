#ifndef NDN_TBUCKET_H
#define NDN_TBUCKET_H
using namespace std;
struct  Bucket {
  int HSent=0;
  int MSent=0;
  int LSent=0;
  int HRecv=0;
  int MRecv=0;
  int LRecv=0;
};

extern Bucket TB;

#endif

#include <stdio.h>
#include <cassert>
#include "board.h"
#include "connect.h"
#include "move.h"
#include "shuff.h"

void prtMove(Move m) {
  printf("%c",emit(m.s)); printf("["); prtLcn(m.lcn); printf("]"); }

const int Nbr_offsets[NumNbrs+1]
  = {-Np2G, 1-Np2G, 1, Np2G, Np2G-1, -1,-Np2G};
const int Bridge_offsets[NumNbrs]
  = {-2*Np2G+1, 2-Np2G,   Np2G+1,
      2*Np2G-1, Np2G-2, -(Np2G+1)};

void show_winners(struct Board B, int st, bool useMiai) { 
  int MP[TotalCells]; int MPsize;
  printf("  %c next-move winners\n",emit(st));
  for (int r=0; r<N; r++) {
    for (int k=0; k<r; k++) printf("  ");
    for (int c=0; c<N-r; c++) {
      int lcn = fatten(r,c);
      Move mv(st,lcn);
      if ((B.board[lcn]==EMP)&&(is_win(B,mv,useMiai,MP,MPsize,false))) 
        printf(" *  ");
      else
        printf(" .  ");
    }
    printf("\n");
  }
  printf("\n");
}

bool is_win(struct Board& B, Move mv, bool useMiai, int C[], int& csize, bool vrbs) { 
  assert(B.board[mv.lcn]==EMP);
  int bd_set = BRDR_NIL;
  Board local = B;
  local.move(mv, useMiai, bd_set);
  if (useMiai && has_win(bd_set)) { // compute miai carrier of winning move
    C[0] = mv.lcn; csize = 1;
    for (int q = 0; q < NumNbrs; q++) {
      int xLcn = mv.lcn+Nbr_offsets[q];
      Move mx(mv.s,xLcn);
      if ((local.board[xLcn]==EMP)&&(!local.not_in_miai(mx)))
        C[csize++] = xLcn;
    }
    if (vrbs) {prtLcn(C[0]); printf(" wins carrier size %d\n",csize); }
  }
  return has_win(bd_set);
}

bool Board::not_in_miai(Move mv) { return reply[ndx(mv.s)][mv.lcn]==mv.lcn ; }

void Board::set_miai(int s, int x, int y) { 
  reply[ndx(s)][x] = y; 
  reply[ndx(s)][y] = x; }

void Board::release_miai(Move mv) { // WARNING: does not alter miai connectivity
  int y = reply[ndx(mv.s)][mv.lcn]; 
  reply[ndx(mv.s)][mv.lcn] = mv.lcn; 
  reply[ndx(mv.s)][y] = y; }

void Board::put_stone(Move mv) { 
  assert((board[mv.lcn]==EMP)||(board[mv.lcn]==mv.s)||board[mv.lcn]==TMP); 
  board[mv.lcn] = mv.s; }

void Board::YborderRealign(Move mv, int& cpt, int c1, int c2, int c3) {
  // no need to undo connectivity :)
  // realign Y border miai as shown  ... x = mv.lcn
  //   * * * *       * * * *
  //    2 1 y         . 1 y
  //     x             x 3
  assert(near_edge(c1) && near_edge(c2)); //printf("Y border realign %c",emit(s));
  release_miai(Move(mv.s,c1));
  set_miai(mv.s,c1,c3); assert(not_in_miai(Move(mv.s,c2)));
  //printf(" new miai"); prtLcn(c1); prtLcn(c3); show();
  int xRoot = Find(p,mv.lcn);
  brdr[xRoot] |= brdr[cpt];
  cpt = Union(p,cpt,xRoot);
}

int Board::moveMiaiPart(Move mv, bool useMiai, int& bdset, int cpt) {
// useMiai true... continue with move( )...
  int nbr,nbrRoot; int lcn = mv.lcn; int s = mv.s;
  release_miai(mv); // your own miai, so connectivity ok
  int miReply = reply[ndx(oppnt(s))][lcn];
  if (miReply != lcn) {
    //prtLcn(lcn); printf(" released opponent miai\n"); 
    release_miai(Move(oppnt(s),lcn));
  }
  // avoid directional bridge bias: search in random order
  int x, perm[NumNbrs] = {0, 1, 2, 3, 4, 5}; 
  shuffle_interval(perm,0,NumNbrs-1); 
  for (int t=0; t<NumNbrs; t++) {  // look for miai nbrs
    // in this order 1) connecting to a stone  2) connecting to a side
    x = perm[t]; assert((x>=0)&&(x<NumNbrs));
    nbr = lcn + Bridge_offsets[x]; // nbr via bridge
    int c1 = lcn+Nbr_offsets[x];     // carrier 
    int c2 = lcn+Nbr_offsets[x+1];   // other carrier
    Move mv1(s,c1); 
    Move mv2(s,c2); 
         // y border realign: move the miai so stones are connected *and* adjacent to border
         //   * -         becomes  * m
         //  m m *                - m *
         // g g g g              g g g g     <-  border guards 
    if (board[nbr] == s &&
        board[c1] == EMP &&
        board[c2] == EMP &&
        (not_in_miai(mv1)||not_in_miai(mv2))) {
          if (!not_in_miai(mv1)) {
            if (near_edge(c1) && (near_edge(lcn) || near_edge(nbr)))
              YborderRealign(Move(s,nbr),cpt,c1,reply[ndx(s)][c1],c2);
          }
          else if (!not_in_miai(mv2)) {
            if (near_edge(c2) && (near_edge(lcn) || near_edge(nbr)))
              YborderRealign(Move(s,nbr),cpt,c2,reply[ndx(s)][c2],c1);
         }
         else if (Find(p,nbr)!=Find(p,cpt)) {  // new miai
           nbrRoot = Find(p,nbr);
           brdr[nbrRoot] |= brdr[cpt];
           cpt = Union(p,cpt,nbrRoot); 
           set_miai(s,c1,c2);
           } 
         }
    else if ((board[nbr] == GRD) &&
             (board[c1]  == EMP) &&
             (board[c2]  == EMP) &&
             (not_in_miai(mv1))&&
             (not_in_miai(mv2))) { // new miai
      brdr[cpt] |= brdr[nbr];
      set_miai(s,c1,c2);
      }
  }
  bdset = brdr[cpt];
  return miReply;
}

int Board::move(Move mv, bool useMiai, int& bdset) { //bdset comp. from scratch
// put mv.s on board, update connectivity for mv.s
// WARNING  oppnt(mv.s) connectivity will be broken if mv.s hits oppnt miai
//   useMiai ? miai adjacency : stone adjacency
// return opponent miai reply of mv, will be mv.lcn if no miai
  int nbr,nbrRoot,cpt; int lcn = mv.lcn; int s = mv.s;
  assert(brdr[lcn]==BRDR_NIL);
  put_stone(mv);
  cpt = lcn; // cpt of s group containing lcn
  for (int t=0; t<NumNbrs; t++) {  // look for absolute nbrs
    nbr = lcn + Nbr_offsets[t];
    if (board[nbr] == s) {
      nbrRoot = Find(p,nbr);
      brdr[nbrRoot] |= brdr[cpt];
      cpt = Union(p,cpt,nbrRoot); } 
    else if (board[nbr] == GRD) {
      brdr[cpt] |= brdr[nbr]; }
  }
  if (!useMiai) {
    bdset = brdr[cpt];
    return lcn;       // no miai, so return lcn
  } // else 
  return moveMiaiPart(mv, useMiai, bdset,cpt);
}

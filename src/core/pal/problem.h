/*
 *   libpal - Automated Placement of Labels Library
 *
 *   Copyright (C) 2008 Maxence Laurent, MIS-TIC, HEIG-VD
 *                      University of Applied Sciences, Western Switzerland
 *                      http://www.hes-so.ch
 *
 *   Contact:
 *      maxence.laurent <at> heig-vd <dot> ch
 *    or
 *      eric.taillard <at> heig-vd <dot> ch
 *
 * This file is part of libpal.
 *
 * libpal is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libpal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libpal.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef PAL_PROBLEM_H
#define PAL_PROBLEM_H

#define SIP_NO_FILE


#include "qgis_core.h"
#include <list>
#include <QList>
#include "rtree.hpp"

namespace pal
{

  class LabelPosition;
  class Label;

  /**
   * \class pal::Sol
   * \ingroup core
   * \note not available in Python bindings
   */
  class Sol
  {
    public:
      int *s = nullptr;
      double cost;
  };

  typedef struct _subpart
  {

    /**
     * # of features in problem
     */
    int probSize;

    /**
     * # of features bounding the problem
     */
    int borderSize;

    /**
     *  total # features (prob + border)
     */
    int subSize;

    /**
     * wrap bw sub feat and main feat
     */
    int *sub = nullptr;

    /**
     * sub solution
     */
    int *sol = nullptr;

    /**
     * first feat in sub part
     */
    int seed;
  } SubPart;

  typedef struct _chain
  {
    int degree;
    double delta;
    int *feat = nullptr;
    int *label = nullptr;
  } Chain;

  /**
   * \ingroup core
   * \brief Representation of a labeling problem
   * \class pal::Problem
   * \note not available in Python bindings
   */
  class CORE_EXPORT Problem
  {

      friend class Pal;

    public:
      Problem();

      //Problem(char *lorena_file, bool displayAll);

      ~Problem();

      //! Problem cannot be copied
      Problem( const Problem &other ) = delete;
      //! Problem cannot be copied
      Problem &operator=( const Problem &other ) = delete;

      /**
       * Adds a candidate label position to the problem.
       * \param position label candidate position. Ownership is transferred to Problem.
       * \since QGIS 2.12
       */
      void addCandidatePosition( LabelPosition *position ) { mLabelPositions.append( position ); }

      /////////////////
      // problem inspection functions
      int getNumFeatures() { return nbft; }
      // features counted 0...n-1
      int getFeatureCandidateCount( int i ) { return featNbLp[i]; }
      // both features and candidates counted 0..n-1
      LabelPosition *getFeatureCandidate( int fi, int ci ) { return mLabelPositions.at( featStartId[fi] + ci ); }
      /////////////////


      void reduce();

      /**
       * \brief popmusic framework
       */
      void popmusic();

      /**
       * \brief Test with very-large scale neighborhood
       */
      void chain_search();

      QList<LabelPosition *> getSolution( bool returnInactive );

      PalStat *getStats();

      /* useful only for postscript post-conversion*/
      //void toFile(char *label_file);

      SubPart *subPart( int r, int featseed, int *isIn );

      void initialization();

      double compute_feature_cost( SubPart *part, int feat_id, int label_id, int *nbOverlap );
      double compute_subsolution_cost( SubPart *part, int *s, int *nbOverlap );

      /**
       *  POPMUSIC, chain
       */
      double popmusic_chain( SubPart *part );

      double popmusic_tabu( SubPart *part );

      /**
       *
       * POPMUSIC, Tabu search with  chain'
       *
       */
      double popmusic_tabu_chain( SubPart *part );

      /**
       * \brief Basic initial solution : every feature to -1
       */
      void init_sol_empty();
      void init_sol_falp();

      static bool compareLabelArea( pal::LabelPosition *l1, pal::LabelPosition *l2 );

    private:

      /**
       * How many layers are labelled ?
       */
      int nbLabelledLayers = 0;

      /**
       * Names of the labelled layers
       */
      QStringList labelledLayersName;

      /**
       * # active candidates (remaining after reduce())
       */
      int nblp = 0;

      /**
       * # candidates (all, including)
       */
      int all_nblp = 0;

      /**
       * # feature to label
       */
      int nbft = 0;


      /**
       * if TRUE, special value -1 is prohibited
       */
      bool displayAll = false;

      /**
       * Map extent (xmin, ymin, xmax, ymax)
       */
      double bbox[4];

      double *labelPositionCost = nullptr;
      int *nbOlap = nullptr;

      QList< LabelPosition * > mLabelPositions;

      RTree<LabelPosition *, double, 2, double> *candidates = nullptr; // index all candidates
      RTree<LabelPosition *, double, 2, double> *candidates_sol = nullptr; // index active candidates
      RTree<LabelPosition *, double, 2, double> *candidates_subsol = nullptr; // idem for subparts

      //int *feat;        // [nblp]
      int *featStartId = nullptr; // [nbft]
      int *featNbLp = nullptr;    // [nbft]
      double *inactiveCost = nullptr; //

      Sol *sol = nullptr;         // [nbft]
      int nbActive = 0;

      double nbOverlap = 0.0;

      int *featWrap = nullptr;

      Chain *chain( SubPart *part, int seed );

      Chain *chain( int seed );

      Pal *pal = nullptr;

      void solution_cost();
      void check_solution();
  };

} // namespace

#endif

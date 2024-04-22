/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using EpicCommonUtilities;

namespace MemLeakDiffer
{
    class PooledAllocatorExperiments
    {
        /// <summary>
        /// Runs a series of experiments on pooled allocator reports in Files
        /// Output is to the console.
        /// </summary>
        public static void Run(List<MemLeakFile> Files)
        {
            // PrintOutOptimalBucketSizes();

            // Find all the mem stats sections
            List<MemStatsReportSection> MemStats = new List<MemStatsReportSection>();
            foreach (MemLeakFile File in Files)
            {
                foreach (ReportSection Section in File.Sections)
                {
                    MemStatsReportSection StatSection = Section as MemStatsReportSection;
                    if (StatSection != null)
                    {
                        MemStats.Add(StatSection);
                        break;
                    }
                }
            }

            // Construct some experiments
            int[] StandardSetupSizes = {
            8,   12,   16,   32,   48,   64,   80,   96,  112,  128,  160,  192,  224,  256,  320,  384,  448,  512,  
            640,  768,  896, 1024, 1280, 1536, 1792, 2048, 2560, 3072, 3584, 4096, 5120, 6144, 7168, 8192,10240,12288,14336,16384};
            MemStatsReportSection.MemPoolSet StandardSetup = new MemStatsReportSection.MemPoolSet(65536, StandardSetupSizes);
            MemStatsReportSection.MemPoolSet SmallerBlockSetup = new MemStatsReportSection.MemPoolSet(16384, StandardSetupSizes);

            // 
            int[] MartinRevised = {
			16,			32,			48,			64,			80,			96,			112,			128,			160,
			192,			224,			256,			384,			512,			640,			768,			896,
			1024, 			1280, 			1536, 			1792, 			2048, 			2304, 			2560, 			2816, 
			3072, 			3328, 			3584, 			3840, 			4096, 			4352, 			4608, 			4864, 
			5376, 			5888, 			6144, 			6400, 			7168, 			8192, 			9216, 			10240,
			10752,			12288,			13056,			16384};
            MemStatsReportSection.MemPoolSet MartinSetup = new MemStatsReportSection.MemPoolSet(65536, MartinRevised);
            MemStatsReportSection.MemPoolSet SmallerBlockMartinSetup = new MemStatsReportSection.MemPoolSet(16384, MartinRevised);
            MemStatsReportSection.MemPoolSet LargerBlockMartinSetup = new MemStatsReportSection.MemPoolSet(131072, MartinRevised);


            int[] TweakedMartinRevised = {
			8,              16,			    32,			    48,			    64,			    80,			    96,			    112,
            128,			160,			192,			224,			256,			384,			512,			640,
            768,			896,			1024, 			1280, 			1536, 			1792, 			2048, 			2304,
            2608, /*/25*/	2848, 			3120, 			3440, 			3632, 			3840, 			4096/* /16 */,	4352, /* /15 */
            4608, /*/14*/	5040/* /13 */, 			5456/* /12 */,	5952/* /11 */,	6544/* /10 */,	7280/* /9 */,	8192/* /8 */,
            9360/* /7 */,	10912/* /6 */,	13104/* /5 */,	16384/* /4 */};
            MemStatsReportSection.MemPoolSet TweakedMartinSetup = new MemStatsReportSection.MemPoolSet(65536, TweakedMartinRevised);

            int[] CurrentNormal =
            {
		        256,	512,   	1024,	1424,	1808,	2048,	2336,	2608,
		        2976,	3264,	3632,	3840,	4096,	4368,	4672,	5040,
		        5456,	5952,	6144,	6544,	7280,	8192,	9360,	10240,
		        12288,	13104,	14336,	16384,
            };
            MemStatsReportSection.MemPoolSet FinalNormalSetup = new MemStatsReportSection.MemPoolSet(65536, CurrentNormal);

            int[] CurrentWriteCombine =
            {
		        256,	512,   	1024,	1424,	1808,	2048,	2336,	2608,
		        2976,	3264,	3632,	3840,	4096,	4368,	4672,	5040,
		        5456,	5952,	6144,	6544,	7280,	8192,	9360,	10240,
		        12288,	13104,	14336,	16384,
            };
            MemStatsReportSection.MemPoolSet FinalWriteCombineSetup = new MemStatsReportSection.MemPoolSet(65536, CurrentWriteCombine);

            int[] CurrentVirtualSizes =
            {
		        8,		16,		32,		48,		64,		80,		96,		112,
		        128,	160,	192,	208,	224,	256,	272,	288,
		        336,	384,	512,	592,	640,	768,	896,	1024,
		        1232,	1488,	1632,	1872,	2048,	2336,	2608,	2976,
		        3264,	3632,	3840,	4096,	4368,	4672,	5040,	5456,
		        5952,	6544,	7280,	8192,	9360,	10912,	13104,	16384
            };
            MemStatsReportSection.MemPoolSet FinalVirtualSetup = new MemStatsReportSection.MemPoolSet(65536, CurrentVirtualSizes);

            int[] NewNormal =
            {
		        256,	512,   	1024,	1424,	1808,	2048,	2336,	2608,
		        2976,	3264,	3632,	3840,	4096,	4368,	4672,	5040,
		        5456,	5952,	6144,	6544,	7280,	8192,	9360,	10240,
		        12288,	13104,	14336,	16384,
            };
            MemStatsReportSection.MemPoolSet NewNormalSetup = new MemStatsReportSection.MemPoolSet(65536, NewNormal);

            int[] NewWriteCombine =
            {
		        256,	512,   	1024,	1424,	1808,	2048,	2336,	
                3264,	4096,	6544,	8192,	10240,
		       	13104,	14336,	16384,
            };
            MemStatsReportSection.MemPoolSet NewWriteCombineSetup = new MemStatsReportSection.MemPoolSet(65536, NewWriteCombine);

            int[] NewVirtualSizes =
            {
		        8,		16,		32,		48,		64,		80,		96,		112,
		        128,	160,	192,	208,	224,	256,	272,	288,
		        336,	384,	512,	592,	640,	768,	896,	1024,
		        1232,	1488,	1632,	1872,	2048,	2336,	2608,	2976,
		        3264,	3632,	3840,	4096,	4368,	4672,	5040,	5456,
		        5952,	6544,	7280,	8192,	9360,	10912,	13104,	16384
            };
            MemStatsReportSection.MemPoolSet NewVirtualSetup = new MemStatsReportSection.MemPoolSet(65536, NewVirtualSizes);

            List<SlackReductionExperiment> ExperimentList = new List<SlackReductionExperiment>();
            SlackReductionExperiment BaselineExpr = new SlackReductionExperiment("Baseline", StandardSetup.Clone(), StandardSetup.Clone(), StandardSetup.Clone());
            ExperimentList.Add(BaselineExpr);
            ExperimentList.Add(new SlackReductionExperiment("Current", FinalNormalSetup.Clone(), FinalNormalSetup.Clone(), FinalVirtualSetup.Clone()));
            ExperimentList.Add(new SlackReductionExperiment("Proposed", NewNormalSetup.Clone(), NewWriteCombineSetup.Clone(), NewVirtualSetup.Clone()));

            // Get the total number of bytes allocated
            long TotalGood = 0;
            long TotalWaste = 0;
            foreach (MemStatsReportSection MLC in MemStats)
            {
                foreach (MemStatsReportSection.MemPoolSet pool in MLC.Pools.Values)
                {
                    int Good;
                    int Waste;
                    pool.Calculate(out Good, out Waste);

                    TotalGood += Good;
                    TotalWaste += Waste;
                }
            }

            Console.WriteLine("");
            Console.WriteLine("Averages on true data.");
            Console.WriteLine("----------------------");
            Console.WriteLine("Good bytes = {0:N1} KB", TotalGood / (1024.0 * MemStats.Count));
            Console.WriteLine("True waste = {0:N1} KB", TotalWaste / (1024.0 * MemStats.Count));

            // Run through each experiment on every open file
            bool bDoneBaseline = false;
            foreach (SlackReductionExperiment Experiment in ExperimentList)
            {
                Console.WriteLine("");
                Console.WriteLine("Experiment " + Experiment.Name);
                Console.WriteLine("-----------------");

                StatisticalFloat TotalExperimentWaste = new StatisticalFloat();
                StatisticalFloat TotalBaselineWaste = new StatisticalFloat();
                foreach (string PoolType in Experiment.PoolConfigs.Keys)
                {
                    MemStatsReportSection.MemPoolSet TestPool = Experiment.PoolConfigs[PoolType];
                    StatisticalFloat ExperimentEfficiency = new StatisticalFloat();
                    StatisticalFloat RunWaste = new StatisticalFloat();
                    StatisticalFloat BaselineWaste = new StatisticalFloat();
                    StatisticalFloat Savings = new StatisticalFloat();

                    foreach (MemStatsReportSection mlc in MemStats)
                    {
                        MemStatsReportSection.MemPoolSet SourcePool = mlc.Pools[PoolType];
                        int WasteBaseline;
                        int Waste;
                        int Good;

                        // First run through the 'standard' setup to compensate for slack due to previous deallocations that left blocks beyond the end partially filled
                        // This ensures we don't unfairly penalize the existing system for real-world allocation ordering issues
                        MemStatsReportSection.MemPoolSet Renormalized = StandardSetup.Clone();
                        Renormalized.MakeEmpty();
                        Renormalized.Recast(SourcePool);

                        Renormalized.Calculate(out Good, out WasteBaseline);
                        BaselineWaste.AddSample(WasteBaseline);

                        // Now run the experiment
                        TestPool.MakeEmpty();
                        TestPool.Recast(Renormalized);
                        TestPool.Calculate(out Good, out Waste);
                        RunWaste.AddSample(Waste);

                        // Update 'savings'
                        Savings.AddSample(WasteBaseline - Waste);
                    }

                    if (bDoneBaseline)
                    {
                        Console.WriteLine("CacheType_{0,-12}: {1,-28} is {2}, savings is {3}", PoolType, Experiment.Name + " Waste", RunWaste.ToStringSizesInKB(), Savings.ToStringSizesInKB());
                    }
                    else
                    {
                        Console.WriteLine("CacheType_{0,-12}: {1,-28} is {2}", PoolType, "Baseline Waste", BaselineWaste.ToStringSizesInKB());
                    }

                    TotalExperimentWaste.AddDistribution(RunWaste);
                    TotalBaselineWaste.AddDistribution(BaselineWaste);
                }

                if (bDoneBaseline)
                {
                    Console.WriteLine("Post-experiment waste is " + TotalExperimentWaste.ToStringSizesInKB());
                }
                else
                {
                    Console.WriteLine("Baseline waste is        " + TotalBaselineWaste.ToStringSizesInKB());
                    bDoneBaseline = true;
                }
            }
        }

        /// <summary>
        /// Container class for an allocator bucket size experiment
        /// </summary>
        class SlackReductionExperiment
        {
            public SlackReductionExperiment(string InName, MemStatsReportSection.MemPoolSet InNormal, MemStatsReportSection.MemPoolSet InWritecombined, MemStatsReportSection.MemPoolSet InVirtual)
            {
                PoolConfigs.Add("Virtual", InVirtual);
                PoolConfigs.Add("WriteCombine", InWritecombined);
                PoolConfigs.Add("Normal", InNormal);
                Name = InName;
            }

            public Dictionary<string, MemStatsReportSection.MemPoolSet> PoolConfigs = new Dictionary<string, MemStatsReportSection.MemPoolSet>();
            public string Name;
        }

        /// <summary>
        /// Print out all near optimal bucket sizes given an alignment constraint, page size, and acceptable waste
        /// </summary>
        public static void PrintOutOptimalBucketSizes()
        {
            Console.WriteLine("");
            Console.WriteLine("Best Bucket Sizes");
            Console.WriteLine("-----------------");

            int PageSize = 65536;
            int MaxWasteAcceptable = 767;
            int LargestPooledAlloc = 16384;
            int MinAlignment = 16;

            int LastElemCount = -1;
            for (int Size = LargestPooledAlloc; Size >= 1; --Size)
            {
                int NumElem = PageSize / Size;
                int Waste = PageSize - Size * NumElem;
                if ((Waste <= MaxWasteAcceptable) && (NumElem != LastElemCount) && ((Size & (MinAlignment - 1)) == 0))
                {
                    Console.WriteLine(Size + " wastes " + Waste);
                    LastElemCount = NumElem;
                }
            }
        }
    }
}
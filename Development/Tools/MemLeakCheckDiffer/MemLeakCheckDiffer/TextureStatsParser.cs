/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Diagnostics;
using EpicCommonUtilities;

namespace MemLeakDiffer
{
    class TextureStreamingSection : TrueKeyValuePairSection
    {
		public float TexturesInMemoryCurrent;
		public float TexturesInMemoryTarget;
		public float OverBudget;
		public float PoolMemoryUsed;
		public float LargestFreeMemoryHole;

        public TextureStreamingSection() : base("Current Texture Streaming Stats")
        {
            MyHeading = "TexturePool";
            MyBasePriority = 19;
            bAdditiveMode = false;
            bCanBeFilteredBySize = false;

            AddMappedSample("Textures In Memory, Current (KB)", "CurrentTexturePoolUsage", EStatType.Average);
            AddMappedSample("Textures In Memory, Target (KB)", "DesiredTexturePoolUsage", EStatType.Average);
            AddMappedSample("Over Budget (KB)", "TexturePoolOverBudget", EStatType.Maximum);
            AddMappedSample("Pool Memory Used (KB)", "TexturePoolAllocatedSize", EStatType.Maximum);
            AddMappedSample("Largest free memory hole (KB)", "TexturePoolLargestHole", EStatType.Minimum);
        }

        public override void Cook()
        {
            base.Cook();

            
			TexturesInMemoryCurrent = (float)GetValue("CurrentTexturePoolUsage");
			TexturesInMemoryTarget = (float)GetValue("DesiredTexturePoolUsage");
			OverBudget = (float)GetValue("TexturePoolOverBudget");
			PoolMemoryUsed = (float)GetValue("TexturePoolAllocatedSize");
			LargestFreeMemoryHole = (float)GetValue("TexturePoolLargestHole");
		}
    }

    class TextureStatsParser : SmartParser
    {
        public override ReportSection Parse(string[] Items, ref int i)
        {
            TextureStreamingSection Result = new TextureStreamingSection();
            Result.Parse(Items, ref i);
            return Result;
        }
    }
}
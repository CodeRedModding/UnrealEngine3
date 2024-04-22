
namespace Dashboard.Models
{
    using System.Collections.Generic;
using System.Linq;
    using System;
    

    partial class RiftMCPDataContext
    {
    }

    // TODO Modify function in MCPRepository to work with the game at this level
    // then cache the game rather than (or in addition to) the AggregateSession.
    public class Game
    {
        public IDictionary<int, Session> Sessions;
    }

    public class Session
    {
        public IDictionary<int, Round> Rounds;
    }
    public class Round
    {
        IDictionary<int, Player> Players;
        IDictionary<int, Weapon> Weapons;
    }

    public class TableDetail
    {
        public IDictionary<int, string> HighlightEvents;
        public IDictionary<int, string> PlayerStatEvents;
        public IDictionary<int, string> WeaponStatEvents;
        public IDictionary<int, string> DamageDealtStatEvents;
        public IDictionary<int, string> DamageReceivedStatEvents;
        public IDictionary<int, string> ProjectileStatEvents;
        public IDictionary<int, string> PawnStatEvents; 
        public TableDetail()
        {
            //Initialize HighlightEvents
            HighlightEvents = new Dictionary<int, string>();
            HighlightEvents.Add(10002, "Kills");
            HighlightEvents.Add(10003, "Deaths");
            HighlightEvents.Add(11021, "Suicide");
            HighlightEvents.Add(11007, "DBNO Player");
            HighlightEvents.Add(11008, "Was DBNOd");
            HighlightEvents.Add(11009, "Revives");
            HighlightEvents.Add(11010, "Was Revived");
            HighlightEvents.Add(11000, "Score");
            HighlightEvents.Add(11006, "Assists");
            
            //Initialize PlayerStatEvents
            PlayerStatEvents = new Dictionary<int, string>();
            PlayerStatEvents.Add(10004, "Match Won");
            PlayerStatEvents.Add(10005, "Round Won");
            PlayerStatEvents.Add(11000, "Score");
            PlayerStatEvents.Add(11001, "Awards");
            PlayerStatEvents.Add(10002, "Kills");
            PlayerStatEvents.Add(10003, "Deaths");
            PlayerStatEvents.Add(11021, "Suicide");
            PlayerStatEvents.Add(10006, "Normal Kill");
            PlayerStatEvents.Add(11011, "Executions");
            PlayerStatEvents.Add(11013, "Team Kills");
            PlayerStatEvents.Add(11015, "Gibs");
            PlayerStatEvents.Add(11017, "Headshots");
            PlayerStatEvents.Add(11019, "Bled Out");
            PlayerStatEvents.Add(11042, "Kill Streak");
            PlayerStatEvents.Add(10007, "Was Normal Kill");
            PlayerStatEvents.Add(11012, "Was Executed");
            PlayerStatEvents.Add(11014, "Was Team Killed");
            PlayerStatEvents.Add(11016, "Was Gibbed");
            PlayerStatEvents.Add(11018, "Was Headshot");
            PlayerStatEvents.Add(11020, "Was Bled Out");
            PlayerStatEvents.Add(11006, "Assists");
            PlayerStatEvents.Add(11040, "Hostage Taken");
            PlayerStatEvents.Add(11041, "Taken Hostage");
            PlayerStatEvents.Add(11030, "Chainsaw Duels Won");
            PlayerStatEvents.Add(11031, "Chainsaw Duels Lost");
            PlayerStatEvents.Add(11027, "Grenade Martyrs");
            PlayerStatEvents.Add(10001, "Player Time Alive");
            PlayerStatEvents.Add(11038, "Roadie Run Time");
            PlayerStatEvents.Add(11039, "Time In Cover");
            PlayerStatEvents.Add(11002, "Ammo Crate Pickups");
            PlayerStatEvents.Add(11003, "Shield Pickups");
            PlayerStatEvents.Add(11007, "DBNO Player");
            PlayerStatEvents.Add(11008, "Was DBNO'd");
            PlayerStatEvents.Add(11009, "Revives");
            PlayerStatEvents.Add(11010, "Was Revived");
            PlayerStatEvents.Add(11004, "Knockdowns");
            PlayerStatEvents.Add(11005, "Cringes");
            PlayerStatEvents.Add(11045, "Spotted Enemy");
            PlayerStatEvents.Add(11046, "Was Spotted");
            PlayerStatEvents.Add(11043, "Bag Tag");
            PlayerStatEvents.Add(11044, "Was Bag Tagged");


            //Initialize WeaponStatEvents
            /* EventID	ExpectedType	Name
             11300	22	Reload Super Success
             11301	22	Reload Success
             11302	22	Reload Failed
             11303	22	Reload Skipped
             EventID	ExpectedType	Name
             10202	23	Weapon Damage Dealt
             10204	23	Weapon Damage Received
             10300	22	Weapon Fired
             11200	23	Weapon DBNO
             11201	23	DBNO By Weapon
             11304	22	Weapon Pickup*/
            WeaponStatEvents = new Dictionary<int, string>();
            WeaponStatEvents.Add(10300, "Weapon Fired");
            WeaponStatEvents.Add(11300, "Reload Super Success");
            WeaponStatEvents.Add(11301, "Reload Success");
            WeaponStatEvents.Add(11302, "Reload Failed");
            WeaponStatEvents.Add(11303, "Reload Skipped");
            WeaponStatEvents.Add(11304, "Weapon Pickup");

            //Initialize DamageDealtStatEvents
            DamageDealtStatEvents = new Dictionary<int, string>();
            DamageDealtStatEvents.Add(10200, "Kills");
            DamageDealtStatEvents.Add(10202, "Weapon Damage Dealt");
            DamageDealtStatEvents.Add(10203, "Melee Damage Dealt");
            DamageDealtStatEvents.Add(11200, "Weapon DBNO");
            DamageDealtStatEvents.Add(11011, "Executions");
            DamageDealtStatEvents.Add(11019, "Bled Out");
            DamageDealtStatEvents.Add(11015, "Gibs");
            DamageDealtStatEvents.Add(11017, "Headshots");

            //Initialize DamageReceivedStatEvents
            DamageReceivedStatEvents = new Dictionary<int, string>();
            DamageReceivedStatEvents.Add(10201, "Deaths");
            DamageReceivedStatEvents.Add(11021, "Suicide");
            DamageReceivedStatEvents.Add(10204, "Weapon Damage Received");
            DamageReceivedStatEvents.Add(10205, "Melee Damage Received");
            DamageReceivedStatEvents.Add(11201, "DBNO By Weapon");
            DamageReceivedStatEvents.Add(10007, "Was Normal Kill");
            DamageReceivedStatEvents.Add(11012, "Was Executed");
            DamageReceivedStatEvents.Add(11020, "Was Bled Out");
            DamageReceivedStatEvents.Add(11014, "Was Team Killed");
            DamageReceivedStatEvents.Add(11016, "Was Gibbed");
            DamageReceivedStatEvents.Add(11018, "Was Headshot");

            //Initialize ProjectilesStatEvents
            // TODO Talk to JoshM and DavidN about these EventIds
            ProjectileStatEvents = new Dictionary<int, string>();
            ProjectileStatEvents.Add(11500, "Grenade Tags");
            ProjectileStatEvents.Add(11501, "Grenade Tagged");
            ProjectileStatEvents.Add(11027, "Grenade Martyrs");
            ProjectileStatEvents.Add(11502, "Mines Planted");
            ProjectileStatEvents.Add(11503, "Mines Tripped");
            ProjectileStatEvents.Add(11504, "Mines I've Tripped");

            //Initialize PawnStatEvents
            PawnStatEvents = new Dictionary<int, string>();
            PawnStatEvents.Add(10400, "Spawns");
            

        }
    }

    // TODO, should probably have a base class for players and weapons
    // that has an id, name, table details, and then a list/dictionary of tabledata and table details

    public class Row
    {
        public int Id;
        public string Name;
        public TableDetail TableDetails;
        public IEnumerable<AEvent> AggregateSession;

        // Houses more information about an event
        // for example the eventID, and a collection of PlayerNames  and their value for that ID
        // Or WeaponNames and their values
        public IDictionary<string, IDictionary<string, int?>> EventDetails;

        // TODO Figure out a good way to house table name Table Details and TableData
        // in a collection so that we can loop through an arbitrary row to create the table
        //public IDictionary<string, IDictionary<string, int?>> Tables;
    }

    public class Weapon : Row
    {

        public IDictionary<string, int?> WeaponStats;
        public IDictionary<string, int?> DamageDealtStats;
        public IDictionary<string, int?> DamageReceivedStats;
        public IDictionary<string, int?> ProjectileStats;
        public IDictionary<string, int?> PawnStats;


        public Weapon()
        {
            this.TableDetails = new TableDetail();
            this.EventDetails = new Dictionary<string, IDictionary<string, int?>>();

            this.WeaponStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.WeaponStatEvents.Values)
            {
                WeaponStats.Add(e, 0);
            }
            
            this.DamageDealtStats = new Dictionary<string, int?>();
            foreach(var e in TableDetails.DamageDealtStatEvents.Values)
            {
                DamageDealtStats.Add(e, 0);
            }

            this.DamageReceivedStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.DamageReceivedStatEvents.Values)
            {
                DamageReceivedStats.Add(e, 0);
            }

            this.ProjectileStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.ProjectileStatEvents.Values)
            {
                ProjectileStats.Add(e, 0);
            }
            this.PawnStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.PawnStatEvents.Values)
            {
                PawnStats.Add(e, 0);
            }
        }
    }
    public class Player : Row
    {

        
        public IDictionary<string, int?> Highlights;
        public IDictionary<string, int?> PlayerStats;
        public IDictionary<string, int?> WeaponStats;
        public IDictionary<string, int?> DamageDealtStats;
        public IDictionary<string, int?> DamageReceivedStats;
        public IDictionary<string, int?> ProjectileStats;
        public IDictionary<string, int?> PawnStats;
        public Player()
        {
            this.TableDetails = new TableDetail();


            this.Highlights = new Dictionary<string, int?>();
            foreach(var e in TableDetails.HighlightEvents.Values)
            {
                Highlights.Add(e, 0);
            }

            this.PlayerStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.PlayerStatEvents.Values)
            {
                PlayerStats.Add(e, 0);
            }

            this.WeaponStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.WeaponStatEvents.Values)
            {
                WeaponStats.Add(e, 0);
            }
            
            this.DamageDealtStats = new Dictionary<string, int?>();
            foreach(var e in TableDetails.DamageDealtStatEvents.Values)
            {
                DamageDealtStats.Add(e, 0);
            }

            this.DamageReceivedStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.DamageReceivedStatEvents.Values)
            {
                DamageReceivedStats.Add(e, 0);
            }

            this.ProjectileStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.ProjectileStatEvents.Values)
            {
                ProjectileStats.Add(e, 0);
            }
            this.PawnStats = new Dictionary<string, int?>();
            foreach (var e in TableDetails.PawnStatEvents.Values)
            {
                PawnStats.Add(e, 0);
            }
        }

    }

    public class AEvent
    {
        public long ID { get; set; }
        public int SessionID { get; set; }
        public Int16 EventType { get; set; }
        public string EventTypeName { get; set; }
        public Int16 EventID { get; set; }
        public string EventName { get; set; }
        public int? TimePeriod { get; set; }
        public int? AggValue { get; set; }
        public Int16? PlayerIndex { get; set; }
        public string PlayerName { get; set; }
        public Int16? StringTableIndex { get; set; }
        public string ClassName { get; set; }

        public AEvent()
        {

        }
    }
}

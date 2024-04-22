/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Web;
using System.Collections;
using System.Collections.Specialized;
using System.Collections.Generic;
using System.Net;
using System.Web.Services;
using System.Web.Services.Protocols;
using AjaxControlToolkit;
using RemotableType;
using UnrealProp;

/// <summary>
/// Summary description for UPWebService
/// </summary>
[WebService( Namespace = "http://tempuri.org/" )]
[WebServiceBinding( ConformsTo = WsiProfiles.BasicProfile1_1 )]
[System.Web.Script.Services.ScriptService]
public class UPWebService : System.Web.Services.WebService
{
    public UPWebService()
    {
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetProjects( string knownCategoryValues, string category )
    {
        List<CascadingDropDownNameValue> Values = new List<CascadingDropDownNameValue>();

        Projects ProjectTable = Global.Project_GetList();
        foreach( Projects.ProjectsRow ProjectRow in ProjectTable.Tables[0].Rows )
        {
            Values.Add( new CascadingDropDownNameValue( ProjectRow.Title, ProjectRow.ID.ToString() ) );
        }

        return( Values.ToArray() );
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetBuilds( string knownCategoryValues, string category )
    {
        List<CascadingDropDownNameValue> Values = new List<CascadingDropDownNameValue>();

        StringDictionary Parameters = CascadingDropDown.ParseKnownCategoryValuesString( knownCategoryValues );
        if( Parameters.ContainsKey( "Project" ) )
        {
            PlatformBuilds PlatformBuildTable = Global.PlatformBuild_GetListForProject( Int16.Parse( Parameters["Project"] ) );
            foreach( PlatformBuilds.PlatformBuildsRow PlatformBuild in PlatformBuildTable.Tables[0].Rows )
            {
                Values.Add( new CascadingDropDownNameValue( PlatformBuild.Title, PlatformBuild.ID.ToString() ) );
            }
        }
        return( Values.ToArray() );
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetPlatformsForBuild( string knownCategoryValues, string category )
    {
        StringDictionary kv = CascadingDropDown.ParseKnownCategoryValuesString( knownCategoryValues );

        if( kv.ContainsKey( "Title" ) )
        {
            List<CascadingDropDownNameValue> values = new List<CascadingDropDownNameValue>();

            PlatformBuilds p = Global.PlatformBuild_GetListForBuild( Int64.Parse( kv["Title"] ) );
            foreach( PlatformBuilds.PlatformBuildsRow dr in p.Tables[0].Rows )
            {
                values.Add( new CascadingDropDownNameValue( dr.Platform, dr.PlatformID.ToString() ) );
            }
            return values.ToArray();
        }

        return null;
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetPlatforms( string knownCategoryValues, string category )
    {
        List<CascadingDropDownNameValue> Values = new List<CascadingDropDownNameValue>();

        Platforms p = Global.Platform_GetList();
        foreach( Platforms.PlatformsRow dr in p.Tables[0].Rows )
        {
            Values.Add( new CascadingDropDownNameValue( dr.Name, dr.ID.ToString() ) );
        }

        return Values.ToArray();
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetPlatformsForProject( string knownCategoryValues, string category )
    {
        StringDictionary kv = CascadingDropDown.ParseKnownCategoryValuesString( knownCategoryValues );
        if( kv.ContainsKey( "Project" ) )
        {
            List<CascadingDropDownNameValue> values = new List<CascadingDropDownNameValue>();

            Platforms p = Global.Platform_GetListForProject( Int16.Parse( kv["Project"] ) );
            foreach( Platforms.PlatformsRow dr in p.Tables[0].Rows )
            {
                values.Add( new CascadingDropDownNameValue( dr.Name, dr.ID.ToString() ) );
            }
            return values.ToArray();
        }
        return null;
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetClientMachinesForPlatform( string knownCategoryValues, string category )
    {
        StringDictionary kv = CascadingDropDown.ParseKnownCategoryValuesString( knownCategoryValues );

        if( kv.ContainsKey( "Platform" ) )
        {
            List<CascadingDropDownNameValue> values = new List<CascadingDropDownNameValue>();

            ClientMachines p = Global.ClientMachine_GetListForPlatform( Int16.Parse( kv["Platform"] ) );
            foreach( ClientMachines.ClientMachinesRow dr in p.Tables[0].Rows )
            {
                values.Add( new CascadingDropDownNameValue( dr.Name, dr.ID.ToString() ) );
            }
            return values.ToArray();
        }

        return null;
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetDistributionServers( string KnownCategoryValues, string Category )
    {
        List<CascadingDropDownNameValue> Values = new List<CascadingDropDownNameValue>();

        string[] Servers = Global.DistributionServer_GetConnectedList();
        foreach( string Server in Servers )
        {
            Values.Add( new CascadingDropDownNameValue( Server, Server ) );
            return Values.ToArray();
        }
        return null;
    }

    [WebMethod]
    public CascadingDropDownNameValue[] GetTaskStatuses( string knownCategoryValues, string category )
    {
        List<CascadingDropDownNameValue> values = new List<CascadingDropDownNameValue>();

        TaskStatuses p = Global.TaskStatus_GetList();
        foreach( TaskStatuses.TaskStatusesRow dr in p.Tables[0].Rows )
        {
            values.Add( new CascadingDropDownNameValue( dr.Description, dr.ID.ToString() ) );
        }

        return values.ToArray();
    }
}


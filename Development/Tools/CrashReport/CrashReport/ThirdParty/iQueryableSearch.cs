﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Dynamic;
using System.Reflection;

namespace Naspinski.IQueryableSearch
{
    public static class IQueryableSearch
    {
        public enum StringSearchType { Contains, Equals };

        /// <summary>
        /// Search an IQueryable's fields for the keywords (objects) - this does not iterate through levels of an object;
        /// This will search the specified fields (properties) by their inferred types, objects in keywords[] that are not of the types in the specified properties will be ignored.
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="properties_to_search">string array of field names</param>
        /// <param name="keywords">objects to search for</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, string[] properties_to_search, object[] keywords)
        {
            list_to_search.NotNull();
            Dictionary<string, Type> dic = new Dictionary<string, Type>();
            foreach (var item in list_to_search.Take(1))
            {
                foreach (PropertyInfo pi in item.GetType().GetProperties())
                    if (properties_to_search.Contains(pi.Name)) dic.Add(pi.Name, pi.PropertyType);
            }
            return list_to_search.Search(dic, keywords);
        }

        /// <summary>
        /// Search an IQueryable's fields for the keywords (objects) - this does not iterate through levels of an object
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="keywords">objects to search for</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, object[] keywords)
        {
            list_to_search.NotNull();
            List<Type> types = new List<Type>();
            foreach (object o in keywords) if (!types.Contains(o.GetType())) types.Add(o.GetType());
            return list_to_search.Search(types.ToArray(), keywords);
        }

        /// <summary>
        /// Search an IQueryable's specified fields for the keywords (objects) - this will iterate down into specified 'types_to_explore'
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="column_names">string array of field names</param>
        /// <param name="keywords">objects to search for</param>
        /// <param name="types_to_explore">the Types that you want to iterate down through</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        /// <returns></returns>
        public static IQueryable Search(this IQueryable list_to_search, string[] properties_to_search, object[] keywords, Type[] types_to_explore)
        {
            list_to_search.NotNull();
            Dictionary<string, Type> dic = new Dictionary<string, Type>();
            foreach (var item in list_to_search.Take(1))
                foreach (PropertyInfo pi in item.GetType().GetProperties()) if (properties_to_search.Contains(pi.Name)) dic.InsertProperty(pi, string.Empty, types_to_explore, properties_to_search);
            return list_to_search.Search(dic, keywords);
        }

        /// <summary>
        /// Search an IQueryable's fields for the keywords (objects) - this will iterate down into specified 'types_to_explore'
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="keywords">objects to search for</param>
        /// <param name="types_to_explore">the Types that you want to iterate down through</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, object[] keywords, Type[] types_to_explore)
        {
            list_to_search.NotNull();
            Dictionary<string, Type> dic = new Dictionary<string, Type>();
            foreach (var item in list_to_search.Take(1))
                foreach (PropertyInfo pi in item.GetType().GetProperties()) dic.InsertProperty(pi, string.Empty, types_to_explore, null);
            return list_to_search.Search(dic, keywords);
        }



        /// <summary>
        /// Search an IQueryable's fields for the keywords (objects) - this does not iterate through levels of an object
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="types_to_explore">array of types to search, others will be ignored</param>
        /// <param name="keywords">objects to search for</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, Type[] types_to_explore, object[] keywords)
        {
            list_to_search.NotNull();
            Dictionary<string, Type> dic = new Dictionary<string, Type>();
            foreach (var item in list_to_search.Take(1))
                foreach (PropertyInfo pi in item.GetType().GetProperties()) if (types_to_explore.Contains(pi.PropertyType)) dic.Add(pi.Name, pi.PropertyType);
            return list_to_search.Search(dic, keywords);
        }

        /// <summary>
        /// Search an IQueryable's specified text fields if they contain the keywords; will not work for any fields other than string fields, if you ware using fields that are not strings, use on of the more specific overloads
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="properties_to_search">string names of the properties to be searched within the IQueryable, may be nest relations as well</param>
        /// <param name="keywords">array for strings to search for</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, string[] properties_to_search, string[] keywords)
        {
            list_to_search.NotNull();
            return Search(list_to_search, properties_to_search, keywords, StringSearchType.Contains);
        }

        /// <summary>
        /// Search an IQueryable's specified text fields if they contain/equal the keywords;  will not work for any fields other than string fields, if you ware using fields that are not strings, use on of the more specific overloads
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="properties_to_search">string names of the properties to be searched within the IQueryable, may be nest relations as well</param>
        /// <param name="keywords">array for strings to search for</param>
        /// <param name="string_search_type">Whether or not the string operations use the strict 'Equals' or the broader 'Contains' method</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, string[] properties_to_search, string[] keywords, StringSearchType string_search_type)
        {
            list_to_search.NotNull();
            return Search(list_to_search, MakeDictionary(properties_to_search), keywords, string_search_type);
        }

        /// <summary>
        /// Search an IQueryable's specified fields if they contain/equal the keywords; fields other than text will be forced to an '==' operator; strings will default to Contains with this overload
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="properties_to_search">Dictionary with KeyValuePairs of [column_name, type_of_column]; such as [record_id, typeof(int)] or [birthdate, typeof(DateTime)]</param>
        /// <param name="keywords">array of objects of 'keywords' to search for, any type of objects</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, Dictionary<string, Type> properties_to_search, object[] keywords)
        {
            list_to_search.NotNull();
            return Search(list_to_search, properties_to_search, keywords, StringSearchType.Contains);
        }

        /// <summary>
        /// Search an IQueryable's specified fields if they contain/equal the keywords; fields other than text will be forced to an '==' operator
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="properties_to_search">Dictionary with KeyValuePairs of [column_name, type_of_column]; such as [record_id, typeof(int)] or [birthdate, typeof(DateTime)]</param>
        /// <param name="keywords">array of objects of 'keywords' to search for, any type of objects</param>
        /// <param name="string_search_type">Whether or not the string operations use the strict 'Equals' or the broader 'Contains' method</param>
        /// <returns>IQueryable of the inputed type filtered by the search specifications</returns>
        public static IQueryable Search(this IQueryable list_to_search, Dictionary<string, Type> properties_to_search, object[] keywords, StringSearchType string_search_type)
        {
            list_to_search.NotNull();
            Dictionary<object, string> search_object_combos = new Dictionary<object, string>();

            foreach (object o in keywords)
            {
                string where_expression = string.Empty;
                foreach (KeyValuePair<string, Type> column in properties_to_search)
                {
                    if (o.GetType() == column.Value)
                    {
                        if (column.Value == typeof(string))
                            where_expression += column.Key + "." + (string_search_type == StringSearchType.Equals ? "Equals" : "Contains") + "(@0) || ";
                        else // any other data types will run against the keyword with a '=='
                            where_expression += column.Key + " == @0 || ";
                    }
                }
                search_object_combos.AddSearchObjectCombo(where_expression, o);
            }

            IQueryable results;
            if (search_object_combos.Count() == 0) results = null; //nothing to search
            else results = list_to_search.SearchInitial(search_object_combos.First().Value, search_object_combos.First().Key);

            if (search_object_combos.Count() > 1)
            {   // otherwise, keep use the resulting set and recursively filter it
                search_object_combos.Remove(search_object_combos.First().Key);
                foreach (KeyValuePair<object, string> combo in search_object_combos)
                    results = Search(results, combo.Value, combo.Key);
            }
            return results;
        }

        /// <summary>
        /// Inserts a [string,Type] combo into the given dictionary based on the inputs
        /// </summary>
        /// <param name="dic">Dictionary holding the search params</param>
        /// <param name="pi">PropertyInfo of the property to attempt to add</param>
        /// <param name="root">string root of the element, "" if it is the first level</param>
        /// <param name="types_to_explore">Types to explore into</param>
        private static void InsertProperty(this Dictionary<string, Type> dic, PropertyInfo pi, string root, Type[] types_to_explore, string[] properties_to_explore)
        {
            if (types_to_explore.Contains(pi.PropertyType))
            {
                string column_name = root + pi.Name + ".";
                foreach (PropertyInfo p in pi.PropertyType.GetProperties())
                    dic.InsertProperty(p, column_name, types_to_explore, properties_to_explore);
            }
            else
                if (properties_to_explore == null || properties_to_explore.Contains(pi.Name)) dic.Add(root + pi.Name, pi.PropertyType);
        }

        /// <summary>
        /// Test for empty IQueryables
        /// </summary>
        /// <param name="list">IQueryable that will be tested to make sure it is not empty</param>
        private static void NotNull(this IQueryable list)
        {
            if (list.Count() < 1) throw new NullReferenceException("list_to_search is empty");
        }

        /// <summary>
        /// Adds a where_expression and object to a Dictionary used to hold the information for a search query
        /// </summary>
        /// <param name="dic">Dictionary for holding combinations of where_expressions and objects</param>
        /// <param name="where_expression">LINQ where expression</param>
        /// <param name="o">object corresponding to the where_expression</param>
        private static void AddSearchObjectCombo(this Dictionary<object, string> dic, string where_expression, object o)
        {
            // remove the last " || "
            if (where_expression.Length > 4) where_expression = where_expression.Remove(where_expression.Length - 4, 4);
            else throw new NullReferenceException("Not searching for anything", new Exception("this often caused by searching for a specific type, but not including a field with that type in the 'list_to_search'"));
            dic.Add(o, where_expression);
        }

        /// <summary>
        /// Called by the public Search function recursively after the initial search is made
        /// </summary>
        /// <param name="results">Results set from the previous search</param>
        /// <param name="where_expression">LINQ where expression</param>
        /// <param name="keyword">object corresponding to the where_expression</param>
        /// <returns>IQueryable of the inputed type filtered by this search specification</returns>
        private static IQueryable Search(IQueryable results, string where_expression, object keyword)
        {
            return results.Where(where_expression, keyword);
        }

        /// <summary>
        /// This will initially search the IQueryable, if you are using Linq-to-SQL or Linq-to-Entites, this will be your only DB call
        /// </summary>
        /// <param name="list_to_search">IQueryable to search</param>
        /// <param name="where_expression">LINQ where expression</param>
        /// <param name="keyword">object corresponding to the where_expression</param>
        /// <returns>IQueryable of the inputed type filtered by this search specification</returns>
        private static IQueryable SearchInitial(this IQueryable list_to_search, string where_expression, object keyword)
        {
            return list_to_search.Where(where_expression, keyword);
        }

        /// <summary>
        /// Takes in a string[] and outputs a corresponding Dictionary[string, typeof(string)] to feed through the search
        /// </summary>
        /// <param name="properties_to_search">array of column names</param>
        /// <returns>Dictionary[string, typeof(string)]</returns>
        private static Dictionary<string, Type> MakeDictionary(string[] properties_to_search)
        {
            Dictionary<string, Type> properties = new Dictionary<string, Type>();
            foreach (string s in properties_to_search)
                if (!properties.ContainsKey(s)) properties.Add(s, typeof(string));
            return properties;
        }
    }
}
$aircraft = Import-Csv -Path ./aircraft_data.tsv -Delimiter "`t"
$length = (Get-Content ./aircraft_data.tsv).length - 1 # -1 for header

if ( $aircraft.length -ne $length )
{
	Write-Output "Mismatched length, aircraft tsv may be malformatted"
	exit 1
}

if (Select-String -Path ./aircraft_data.tsv -Pattern " ")
{
	Write-Output "Spaces are present in the data file."
	exit 1
}

foreach ($plane in $aircraft)
{
	try {
		if ( $plane.ICAO_Code -isnot [string] )
		{
			Write-Output "Aircraft ICAO not a string: " $plane.ICAO_Code
			exit 1
		}

		if ( [double]$plane.Wingspan -isnot [double])
		{
			Write-Output "Aircraft Wingspan malformed " $plane.ICAO_Code $plane.Wingspan $plane.Wingspan.GetType()
			exit 1
		}

		if ( [double]$plane.Length -isnot [double])
		{
			Write-Output "Aircraft Length malformed " $plane.ICAO_Code $plane.Length $plane.Length.GetType()
			exit 1
		}

		if ( [double]$plane.Tail -isnot [double])
		{
			Write-Output "Aircraft Tail malformed " $plane.ICAO_Code $plane.Tail $plane.Tail.GetType()
			exit 1
		}

		if ( [double]$plane.Width -isnot [double])
		{
			Write-Output "Aircraft Width malformed " $plane.ICAO_Code $plane.Width $plane.Width.GetType()
			exit 1
		}
	}
	catch
	{
		Write-Output "Malformed record: " $plane
		exit 1
	}
}

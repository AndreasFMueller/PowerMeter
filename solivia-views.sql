--
-- solivia-views.sql
--
-- (c) 2023 Prof Dr Andreas Müller
--

create view gridpower as
select d.timekey, s.stationid, sum(d.value) as 'power'
from sdata d, sensor s, mfield m
where d.sensorid = s.id
  and d.fieldid = m.id
  and s.name like 'phase%'
  and m.name = 'power'
group by d.timekey, s.stationid;

create view solarpower as
select d.timekey, s.stationid, sum(d.value) as 'power'
from sdata d, sensor s, mfield m
where d.sensorid = s.id
  and d.fieldid = m.id
  and s.name like 'string%'
  and m.name = 'power'
group by d.timekey, s.stationid;

create view power as
select g.timekey, g.stationid,
       g.power as gridpower, s.power as solarpower,
       g.power / s.power as efficiency
from gridpower g, solarpower s
where g.timekey = s.timekey
  and g.stationid = s.stationid;



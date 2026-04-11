-- 1. TABLE
create table public.device_control (
  id integer primary key,
  device_name text default 'ESP32 LED',
  led_status boolean not null default false,
  updated_at timestamp with time zone default now()
);

-- 2. INITIAL DATA
insert into public.device_control (id, led_status)
values (1, false);

-- 3. AUTO UPDATE TIMESTAMP
create or replace function update_timestamp()
returns trigger as $$
begin
  new.updated_at = now();
  return new;
end;
$$ language plpgsql;

create trigger set_timestamp
before update on public.device_control
for each row
execute function update_timestamp();

-- 4. ENABLE RLS
alter table public.device_control enable row level security;

-- 5. POLICIES (IMPORTANT)
create policy "Public read"
on public.device_control
for select
using (true);

create policy "Public update"
on public.device_control
for update
using (true);

-- 6. ENABLE REALTIME
alter publication supabase_realtime add table public.device_control;